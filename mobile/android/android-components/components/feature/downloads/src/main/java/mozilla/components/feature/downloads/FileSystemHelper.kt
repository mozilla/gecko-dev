/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.os.StatFs
import java.io.File

/**
 * Interface for abstracting file system operations.
 * This allows for easier testing by providing a way to mock file system interactions.
 */
interface FileSystemHelper {

    /**
     * Creates a directory at the specified path if it does not already exist.
     *
     * @param path The absolute path of the directory to create.
     * @return `true` if the directory was created, `false` if it already existed.
     */
    fun createDirectoryIfNotExists(path: String): Boolean

    /**
     * Checks if the given path points to an existing directory.
     *
     * @param path The absolute path to check.
     * @return `true` if the path is an existing directory, `false` otherwise.
     */
    fun isDirectory(path: String): Boolean

    /**
     * Returns the number of available bytes in the file system directory specified by the path.
     *
     * @param path The absolute path of the directory.
     * @return The number of available bytes, or 0 if the path does not exist or is not a directory.
     */
    fun availableBytesInDirectory(path: String): Long
}

/**
 * Default implementation of [FileSystemHelper].
 */
class DefaultFileSystemHelper : FileSystemHelper {

    /**
     * Creates a directory at the specified path if it does not already exist.
     * It uses [File.mkdirs] which creates parent directories if necessary.
     *
     * @param path The absolute path of the directory to create.
     * @return `true` if the directory was created (or any necessary parent directories),
     *         `false` if the directory already existed.
     */
    override fun createDirectoryIfNotExists(path: String): Boolean {
        val directory = File(path)
        if (!directory.exists()) {
            return directory.mkdirs()
        }
        return false
    }

    /**
     * Returns the number of available bytes in the file system directory specified by the path.
     * Uses [StatFs] to retrieve this information.
     *
     * @param path The absolute path of the directory.
     * @return The number of available bytes.
     * @throws IllegalArgumentException if the path does not exist.
     */
    override fun availableBytesInDirectory(path: String): Long = StatFs(path).availableBytes

    /**
     * Checks if the given path points to an existing directory.
     *
     * @param path The absolute path to check.
     * @return `true` if the path exists and is a directory, `false` otherwise.
     */
    override fun isDirectory(path: String): Boolean {
        val file = File(path)
        return file.exists() && file.isDirectory
    }
}
