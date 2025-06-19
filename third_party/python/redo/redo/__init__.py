# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import asyncio
import functools
import logging
import random
import time
from contextlib import contextmanager
from typing import Any, Awaitable, Callable, Dict, Optional, Sequence, Tuple, Type, Union

log = logging.getLogger(__name__)


def retrier(attempts=5, sleeptime=10, max_sleeptime=300, sleepscale=1.5, jitter=1):
    """
    A generator function that sleeps between retries, handles exponential
    backoff and jitter. The action you are retrying is meant to run after
    retrier yields.

    At each iteration, we sleep for sleeptime + random.uniform(-jitter, jitter).
    Afterwards sleeptime is multiplied by sleepscale for the next iteration.

    Args:
        attempts (int): maximum number of times to try; defaults to 5
        sleeptime (float): how many seconds to sleep between tries; defaults to
                           10 seconds
        max_sleeptime (float): the longest we'll sleep, in seconds; defaults to
                               300s (five minutes)
        sleepscale (float): how much to multiply the sleep time by each
                            iteration; defaults to 1.5
        jitter (float): random jitter to introduce to sleep time each iteration.
                      the amount is chosen at random between [-jitter, +jitter]
                      defaults to 1

    Yields:
        None, a maximum of `attempts` number of times

    Example:
        >>> n = 0
        >>> for _ in retrier(sleeptime=0, jitter=0):
        ...     if n == 3:
        ...         # We did the thing!
        ...         break
        ...     n += 1
        >>> n
        3

        >>> n = 0
        >>> for _ in retrier(sleeptime=0, jitter=0):
        ...     if n == 6:
        ...         # We did the thing!
        ...         break
        ...     n += 1
        ... else:
        ...     print("max tries hit")
        max tries hit
    """
    jitter = jitter or 0  # py35 barfs on the next line if jitter is None
    if jitter > sleeptime:
        # To prevent negative sleep times
        raise Exception("jitter ({}) must be less than sleep time ({})".format(jitter, sleeptime))

    sleeptime_real = sleeptime
    for _ in range(attempts):
        log.debug("attempt %i/%i", _ + 1, attempts)

        yield sleeptime_real

        if jitter:
            sleeptime_real = sleeptime + random.uniform(-jitter, jitter)
            # our jitter should scale along with the sleeptime
            jitter = jitter * sleepscale
        else:
            sleeptime_real = sleeptime

        sleeptime *= sleepscale

        if sleeptime_real > max_sleeptime:
            sleeptime_real = max_sleeptime

        # Don't need to sleep the last time
        if _ < attempts - 1:
            log.debug("sleeping for %.2fs (attempt %i/%i)", sleeptime_real, _ + 1, attempts)
            time.sleep(sleeptime_real)


def retry(
    action,
    attempts=5,
    sleeptime=60,
    max_sleeptime=5 * 60,
    sleepscale=1.5,
    jitter=1,
    retry_exceptions=(Exception,),
    cleanup=None,
    args=(),
    kwargs={},
    log_args=True,
):
    """
    Calls an action function until it succeeds, or we give up.

    Args:
        action (callable): the function to retry
        attempts (int): maximum number of times to try; defaults to 5
        sleeptime (float): how many seconds to sleep between tries; defaults to
                           60s (one minute)
        max_sleeptime (float): the longest we'll sleep, in seconds; defaults to
                               300s (five minutes)
        sleepscale (float): how much to multiply the sleep time by each
                            iteration; defaults to 1.5
        jitter (float): random jitter to introduce to sleep time each iteration.
                      the amount is chosen at random between [-jitter, +jitter]
                      defaults to 1
        retry_exceptions (tuple): tuple of exceptions to be caught. If other
                                  exceptions are raised by action(), then these
                                  are immediately re-raised to the caller.
        cleanup (callable): optional; called if one of `retry_exceptions` is
                            caught. No arguments are passed to the cleanup
                            function; if your cleanup requires arguments,
                            consider using functools.partial or a lambda
                            function.
        args (tuple): positional arguments to call `action` with
        kwargs (dict): keyword arguments to call `action` with
        log_args (bool): whether or not to include args and kwargs in log
                         messages. Defaults to True.

    Returns:
        Whatever action(*args, **kwargs) returns

    Raises:
        Whatever action(*args, **kwargs) raises. `retry_exceptions` are caught
        up until the last attempt, in which case they are re-raised.

    Example:
        >>> count = 0
        >>> def foo():
        ...     global count
        ...     count += 1
        ...     print(count)
        ...     if count < 3:
        ...         raise ValueError("count is too small!")
        ...     return "success!"
        >>> retry(foo, sleeptime=0, jitter=0)
        1
        2
        3
        'success!'
    """
    assert callable(action)
    assert not cleanup or callable(cleanup)

    action_name = getattr(action, "__name__", action)
    if log_args and (args or kwargs):
        log_attempt_args = ("retry: calling %s with args: %s," " kwargs: %s, attempt #%d", action_name, args, kwargs)
    else:
        log_attempt_args = ("retry: calling %s, attempt #%d", action_name)

    if max_sleeptime < sleeptime:
        log.debug("max_sleeptime %d less than sleeptime %d", max_sleeptime, sleeptime)

    n = 1
    for _ in retrier(attempts=attempts, sleeptime=sleeptime, max_sleeptime=max_sleeptime, sleepscale=sleepscale, jitter=jitter):
        try:
            logfn = log.info if n != 1 else log.debug
            logfn_args = log_attempt_args + (n,)
            logfn(*logfn_args)
            return action(*args, **kwargs)
        except retry_exceptions:
            log.debug("retry: Caught exception: ", exc_info=True)
            if cleanup:
                cleanup()
            if n == attempts:
                log.info("retry: Giving up on %s", action_name)
                raise
            continue
        finally:
            n += 1


def retriable(*retry_args, **retry_kwargs):
    """
    A decorator factory for retry(). Wrap your function in @retriable(...) to
    give it retry powers!

    Arguments:
        Same as for `retry`, with the exception of `action`, `args`, and `kwargs`,
        which are left to the normal function definition.

    Returns:
        A function decorator

    Example:
        >>> count = 0
        >>> @retriable(sleeptime=0, jitter=0)
        ... def foo():
        ...     global count
        ...     count += 1
        ...     print(count)
        ...     if count < 3:
        ...         raise ValueError("count too small")
        ...     return "success!"
        >>> foo()
        1
        2
        3
        'success!'
    """

    def _retriable_factory(func):
        @functools.wraps(func)
        def _retriable_wrapper(*args, **kwargs):
            return retry(func, args=args, kwargs=kwargs, *retry_args, **retry_kwargs)

        return _retriable_wrapper

    return _retriable_factory


@contextmanager
def retrying(func, *retry_args, **retry_kwargs):
    """
    A context manager for wrapping functions with retry functionality.

    Arguments:
        func (callable): the function to wrap
        other arguments as per `retry`

    Returns:
        A context manager that returns retriable(func) on __enter__

    Example:
        >>> count = 0
        >>> def foo():
        ...     global count
        ...     count += 1
        ...     print(count)
        ...     if count < 3:
        ...         raise ValueError("count too small")
        ...     return "success!"
        >>> with retrying(foo, sleeptime=0, jitter=0) as f:
        ...     f()
        1
        2
        3
        'success!'
    """
    yield retriable(*retry_args, **retry_kwargs)(func)


def calculate_sleep_time(attempt, delay_factor=5.0, randomization_factor=0.5, max_delay=120):
    """Calculate the sleep time between retries, in seconds.

    Based off of `taskcluster.utils.calculateSleepTime`, but with kwargs instead
    of constant `delay_factor`/`randomization_factor`/`max_delay`.  The taskcluster
    function generally slept for less than a second, which didn't always get
    past server issues.
    Args:
        attempt (int): the retry attempt number
        delay_factor (float, optional): a multiplier for the delay time.  Defaults to 5.
        randomization_factor (float, optional): a randomization multiplier for the
            delay time.  Defaults to .5.
        max_delay (float, optional): the max delay to sleep.  Defaults to 120 (seconds).
    Returns:
        float: the time to sleep, in seconds.
    """
    if attempt <= 0:
        return 0

    # We subtract one to get exponents: 1, 2, 3, 4, 5, ..
    delay = float(2 ** (attempt - 1)) * float(delay_factor)
    # Apply randomization factor.  Only increase the delay here.
    delay = delay * (randomization_factor * random.random() + 1)
    # Always limit with a maximum delay
    return min(delay, max_delay)


async def retry_async(
    func: Callable[..., Awaitable[Any]],
    attempts: int = 5,
    sleeptime_callback: Callable[..., Any] = calculate_sleep_time,
    retry_exceptions: Union[Type[BaseException], Tuple[Type[BaseException], ...]] = Exception,
    args: Sequence[Any] = (),
    kwargs: Optional[Dict[str, Any]] = None,
    sleeptime_kwargs: Optional[Dict[str, Any]] = None,
) -> Any:
    """Retry ``func``, where ``func`` is an awaitable.

    Args:
        func (function): an awaitable function.
        attempts (int, optional): the number of attempts to make.  Default is 5.
        sleeptime_callback (function, optional): the function to use to determine
            how long to sleep after each attempt.  Defaults to ``calculateSleepTime``.
        retry_exceptions (list or exception, optional): the exception(s) to retry on.
            Defaults to ``Exception``.
        args (list, optional): the args to pass to ``func``.  Defaults to ()
        kwargs (dict, optional): the kwargs to pass to ``func``.  Defaults to
            {}.
        sleeptime_kwargs (dict, optional): the kwargs to pass to ``sleeptime_callback``.
            If None, use {}.  Defaults to None.
    Returns:
        object: the value from a successful ``function`` call
    Raises:
        Exception: the exception from a failed ``function`` call, either outside
            of the retry_exceptions, or one of those if we pass the max
            ``attempts``.
    """
    kwargs = kwargs or {}
    attempt = 1
    while True:
        try:
            return await func(*args, **kwargs)
        except retry_exceptions:
            attempt += 1
            _check_number_of_attempts(attempt, attempts, func, "retry_async")
            await asyncio.sleep(_define_sleep_time(sleeptime_kwargs, sleeptime_callback, attempt, func, "retry_async"))


def _check_number_of_attempts(attempt: int, attempts: int, func: Callable[..., Any], retry_function_name: str) -> None:
    if attempt > attempts:
        log.warning("{}: {}: too many retries!".format(retry_function_name, func.__name__))
        raise


def _define_sleep_time(
    sleeptime_kwargs: Optional[Dict[str, Any]],
    sleeptime_callback: Callable[..., int],
    attempt: int,
    func: Callable[..., Any],
    retry_function_name: str,
) -> float:
    sleeptime_kwargs = sleeptime_kwargs or {}
    sleep_time = sleeptime_callback(attempt, **sleeptime_kwargs)
    log.debug("{}: {}: sleeping {} seconds before retry".format(retry_function_name, func.__name__, sleep_time))
    return sleep_time


def retriable_async(
    retry_exceptions: Union[Type[BaseException], Tuple[Type[BaseException], ...]] = Exception,
    sleeptime_kwargs: Optional[Dict[str, Any]] = None,
) -> Callable[..., Callable[..., Awaitable[Any]]]:
    """Decorate a function by wrapping ``retry_async`` around.

    Args:
        retry_exceptions (list or exception, optional): the exception(s) to retry on.
            Defaults to ``Exception``.
        sleeptime_kwargs (dict, optional): the kwargs to pass to ``sleeptime_callback``.
            If None, use {}.  Defaults to None.
    Returns:
        function: the decorated function
    """

    def wrap(async_func: Callable[..., Awaitable[Any]]) -> Callable[..., Awaitable[Any]]:
        @functools.wraps(async_func)
        async def wrapped(*args: Any, **kwargs: Any) -> Any:
            return await retry_async(
                async_func,
                retry_exceptions=retry_exceptions,
                args=args,
                kwargs=kwargs,
                sleeptime_kwargs=sleeptime_kwargs,
            )

        return wrapped

    return wrap
