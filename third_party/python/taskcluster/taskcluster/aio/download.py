"""
Support for downloading objects from the object service, following best
practices for that service.

Downloaded data is written to a "writer" provided by a "writer factory".  A
writer has an async `write` method which writes the entire passed buffer to
storage.  A writer factory is an async callable which returns a fresh writer,
ready to write the first byte of the object.  When downloads are retried, the
writer factory may be called more than once.

Note that `aiofile.open` returns a value suitable for use as a writer, if async
file IO is important to the application.

This module provides several pre-defined writers and writer factories for
common cases.
"""
import aiohttp
import contextlib
import datetime
import hashlib

from dateutil.parser import parse as dateparse

from .asyncutils import ensureCoro
from .reader_writer import streamingCopy, BufferWriter, FileWriter
from .retry import retry
from . import Object
from ..exceptions import TaskclusterArtifactError, TaskclusterFailure, ObjectHashVerificationError

# The subset of hashes supported by HashingWriter which are "accepted" as per
# the object service's schemas.
ACCEPTABLE_HASHES = set(['sha256', 'sha512'])


async def downloadToBuf(**kwargs):
    """
    Convenience method to download data to an in-memory buffer and return the
    downloaded data.  Arguments are the same as `download`, except that
    `writerFactory` should not be supplied.  Returns a tuple (buffer, contentType).
    """
    writer = None

    async def writerFactory():
        nonlocal writer
        writer = BufferWriter()
        return writer

    contentType = await download(writerFactory=writerFactory, **kwargs)
    return writer.getbuffer(), contentType


async def downloadToFile(file, **kwargs):
    """
    Convenience method to download data to a file object.  The file must be
    writeable, in binary mode, seekable (`f.seek`), and truncatable
    (`f.truncate`) to support retries.  Arguments are the same as `download`,
    except that `writerFactory` should not be supplied.  Returns the content-type.
    """
    async def writerFactory():
        file.seek(0)
        file.truncate()
        return FileWriter(file)

    return await download(writerFactory=writerFactory, **kwargs)


async def download(*, name, maxRetries=5, objectService, writerFactory):
    """
    Download the named object from the object service, using a writer returned
    from `writerFactory` to write the data.  The `maxRetries` parameter has
    the same meaning as for service clients.  The `objectService` parameter is
    an instance of the Object class, configured with credentials for the
    download.  Returns the content-type.
    """
    async with aiohttp.ClientSession() as session:
        downloadResp = await ensureCoro(objectService.startDownload)(name, {
            "acceptDownloadMethods": {
                "getUrl": True,
            },
        })

        method = downloadResp["method"]

        if method == "getUrl":
            return await _getUrlDownload(name, downloadResp, objectService, writerFactory, session, maxRetries)
        else:
            raise RuntimeError(f'Unknown download method {method}')


async def _getUrlDownload(name, downloadResp, objectService, writerFactory, session, maxRetries):
    """
    Implementation of the getUrl download method.
    """
    downloadRespUsed = False

    async def tryDownload(retryFor):
        nonlocal downloadResp
        nonlocal downloadRespUsed

        with _maybeRetryHttpRequest(retryFor):
            writer = HashingWriter(await writerFactory())

            # if the downloadResp has been used at least once and has now expired,
            # get a new one before proceeding
            if downloadRespUsed and dateparse(downloadResp["expires"]) < datetime.datetime.utcnow():
                downloadResp = await ensureCoro(objectService.startDownload)(name, {
                    "acceptDownloadMethods": {
                        "getUrl": True,
                    },
                })
                downloadRespUsed = False

            downloadRespUsed = True
            async with session.get(downloadResp["url"]) as resp:
                contentType = resp.content_type
                resp.raise_for_status()
                # note that `resp.content` is a StreamReader and satisfies the
                # requirements of a reader in this case
                await streamingCopy(resp.content, writer)

            # having completed the download, verify the hashes.  Note that a
            # hash verification failure is not retried.
            observedHashes = writer.hashes()
            expectedHashes = downloadResp["hashes"]
            verifyHashes(observedHashes, expectedHashes)

            return contentType

    return await retry(maxRetries, tryDownload)


async def downloadArtifactToBuf(**kwargs):
    """
    Convenience method to download an artifact to an in-memory buffer and return the
    downloaded data.  Arguments are the same as `downloadArtifact`, except that
    `writerFactory` should not be supplied.  Returns a tuple (buffer, contentType).
    """
    writer = None

    async def writerFactory():
        nonlocal writer
        writer = BufferWriter()
        return writer

    contentType = await downloadArtifact(writerFactory=writerFactory, **kwargs)
    return writer.getbuffer(), contentType


async def downloadArtifactToFile(file, **kwargs):
    """
    Convenience method to download an artifact to a file object.  The file must be
    writeable, in binary mode, seekable (`f.seek`), and truncatable
    (`f.truncate`) to support retries.  Arguments are the same as `downloadArtifac`,
    except that `writerFactory` should not be supplied.  Returns the content-type.
    """
    async def writerFactory():
        file.seek(0)
        file.truncate()
        return FileWriter(file)

    return await downloadArtifact(writerFactory=writerFactory, **kwargs)


async def downloadArtifact(*, taskId, name, runId=None, maxRetries=5, queueService, writerFactory):
    """
    Download the named artifact with the appropriate storageType, using a writer returned
    from `writerFactory` to write the data.  The `maxRetries` parameter has
    the same meaning as for service clients.  The `queueService` parameter is
    an instance of the Queue class, configured with credentials for the
    download.  Returns the content-type.
    """
    if runId is None:
        artifact = await ensureCoro(queueService.latestArtifact)(taskId, name)
    else:
        artifact = await ensureCoro(queueService.artifact)(taskId, runId, name)

    if artifact["storageType"] == 's3' or artifact["storageType"] == 'reference':
        async with aiohttp.ClientSession() as session:
            return await _s3Download(artifact["url"], writerFactory, session, maxRetries)

    elif artifact["storageType"] == 'object':
        objectService = Object({
            "rootUrl": queueService.options["rootUrl"],
            "maxRetries": maxRetries,
            "credentials": artifact["credentials"],
        })
        return await download(
            name=artifact["name"],
            maxRetries=maxRetries,
            objectService=objectService,
            writerFactory=writerFactory)

    elif artifact["storageType"] == 'error':
        raise TaskclusterArtifactError(artifact["message"], artifact["reason"])

    else:
        raise TaskclusterFailure(f"Unknown storageType f{artifact['storageType']}")


async def _s3Download(url, writerFactory, session, maxRetries):
    """
    Perform a download from the given S3 URL, including retrying.
    """
    async def tryDownload(retryFor):
        with _maybeRetryHttpRequest(retryFor):
            writer = await writerFactory()

            async with session.get(url) as resp:
                contentType = resp.content_type
                resp.raise_for_status()
                # note that `resp.content` is a StreamReader and satisfies the
                # requirements of a reader in this case
                await streamingCopy(resp.content, writer)

            return contentType

    return await retry(maxRetries, tryDownload)


@contextlib.contextmanager
def _maybeRetryHttpRequest(retryFor):
    "Catch errors from an aiohttp request and retry the retriable responses."
    try:
        yield
    except aiohttp.ClientResponseError as exc:
        # treat 4xx's as fatal, and retry others
        if 400 <= exc.status < 500:
            raise exc
        return retryFor(exc)
    except aiohttp.ClientError as exc:
        # retry for all other aiohttp errors
        return retryFor(exc)
    # .. anything else is considered fatal


def verifyHashes(observed, expected):
    """Verify that the hashes observed on the data stream match those provided
    by the object API, where present, and that at least one acceptable
    algorithm is present."""
    someValidAcceptableHash = False

    for algo, oh in observed.items():
        if algo in expected:
            eh = expected[algo]
            if oh != eh:
                raise ObjectHashVerificationError(f"Validation of object data's {algo} hash failed")
            if algo in ACCEPTABLE_HASHES:
                someValidAcceptableHash = True

    if not someValidAcceptableHash:
        raise ObjectHashVerificationError("No acceptable hashes found in object metadata")


class HashingWriter:
    """A Writer implementation that hashes contents as they are written."""

    def __init__(self, inner):
        self.inner = inner
        self.sha256 = hashlib.sha256()
        self.sha512 = hashlib.sha512()

    async def write(self, chunk):
        await self.inner.write(chunk)
        self.update(chunk)

    def update(self, chunk):
        self.sha256.update(chunk)
        self.sha512.update(chunk)

    def hashes(self):
        """Return the hashes in a format like that used in he object API."""
        return {
            "sha256": self.sha256.hexdigest(),
            "sha512": self.sha512.hexdigest(),
        }
