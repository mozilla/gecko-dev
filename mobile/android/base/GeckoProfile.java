/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.INIParser;
import org.mozilla.gecko.util.INISection;

import android.content.Context;
import android.text.TextUtils;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Hashtable;

public final class GeckoProfile {
    private static final String LOGTAG = "GeckoProfile";
    // Used to "lock" the guest profile, so that we'll always restart in it
    private static final String LOCK_FILE_NAME = ".active_lock";
    public static final String DEFAULT_PROFILE = "default";

    private static HashMap<String, GeckoProfile> sProfileCache = new HashMap<String, GeckoProfile>();
    private static String sDefaultProfileName = null;

    private final String mName;
    private File mProfileDir;
    public static boolean sIsUsingCustomProfile = false;

    // Constants to cache whether or not a profile is "locked".
    private enum LockState {
        LOCKED,
        UNLOCKED,
        UNDEFINED
    };
    // Caches whether or not a profile is "locked". Only used by the guest profile to determine if it should
    // be reused or deleted on startup
    private LockState mLocked = LockState.UNDEFINED;

    // Caches the guest profile dir
    private static File mGuestDir = null;

    private boolean mInGuestMode = false;
    private static GeckoProfile mGuestProfile = null;

    private static final String MOZILLA_DIR_NAME = "mozilla";
    private static File sMozillaDir;

    private static INIParser getProfilesINI(File mozillaDir) {
        File profilesIni = new File(mozillaDir, "profiles.ini");
        return new INIParser(profilesIni);
    }

    public static GeckoProfile get(Context context) {
        boolean isGeckoApp = false;
        try {
            isGeckoApp = context instanceof GeckoApp;
        } catch (NoClassDefFoundError ex) {}
        

        if (isGeckoApp) {
            // Check for a cached profile on this context already
            // TODO: We should not be caching profile information on the Activity context
            if (((GeckoApp)context).mProfile != null) {
                return ((GeckoApp)context).mProfile;
            }
        }

        // If the guest profile exists and is locked, return it
        GeckoProfile guest = GeckoProfile.getGuestProfile(context);
        if (guest != null && guest.locked()) {
            return guest;
        }

        if (isGeckoApp) {
            // Otherwise, get the default profile for the Activity
            return get(context, ((GeckoApp)context).getDefaultProfileName());
        }

        return get(context, "");
    }

    public static GeckoProfile get(Context context, String profileName) {
        synchronized (sProfileCache) {
            GeckoProfile profile = sProfileCache.get(profileName);
            if (profile != null)
                return profile;
        }
        return get(context, profileName, (File)null);
    }

    public static GeckoProfile get(Context context, String profileName, String profilePath) {
        File dir = null;
        if (!TextUtils.isEmpty(profilePath))
            dir = new File(profilePath);
        return get(context, profileName, dir);
    }

    public static GeckoProfile get(Context context, String profileName, File profileDir) {
        if (context == null) {
            throw new IllegalArgumentException("context must be non-null");
        }

        // if no profile was passed in, look for the default profile listed in profiles.ini
        // if that doesn't exist, look for a profile called 'default'
        if (TextUtils.isEmpty(profileName) && profileDir == null) {
            profileName = GeckoProfile.findDefaultProfile(context);
            if (profileName == null)
                profileName = DEFAULT_PROFILE;
        }

        // actually try to look up the profile
        synchronized (sProfileCache) {
            GeckoProfile profile = sProfileCache.get(profileName);
            if (profile == null) {
                profile = new GeckoProfile(context, profileName);
                profile.setDir(profileDir);
                sProfileCache.put(profileName, profile);
            } else {
                profile.setDir(profileDir);
            }
            return profile;
        }
    }

    private static File getMozillaDirectory(Context context) {
        return new File(context.getFilesDir(), MOZILLA_DIR_NAME);
    }

    private synchronized File ensureMozillaDirectory() throws IOException {
        if (sMozillaDir.exists() || sMozillaDir.mkdirs()) {
            return sMozillaDir;
        }

        // Although this leaks a path to the system log, the path is
        // predictable (unlike a profile directory), so this is fine.
        throw new IOException("Unable to create mozilla directory at " + sMozillaDir.getAbsolutePath());
    }

    public static boolean removeProfile(Context context, String profileName) {
        return new GeckoProfile(context, profileName).remove();
    }

    public static GeckoProfile createGuestProfile(Context context) {
        try {
            removeGuestProfile(context);
            // We need to force the creation of a new guest profile if we want it outside of the normal profile path,
            // otherwise GeckoProfile.getDir will try to be smart and build it for us in the normal profiles dir.
            getGuestDir(context).mkdir();
            GeckoProfile profile = getGuestProfile(context);
            profile.lock();
            return profile;
        } catch (Exception ex) {
            Log.e(LOGTAG, "Error creating guest profile", ex);
        }
        return null;
    }

    public static void leaveGuestSession(Context context) {
        GeckoProfile profile = getGuestProfile(context);
        if (profile != null) {
            profile.unlock();
        }
    }

    private static File getGuestDir(Context context) {
        if (mGuestDir == null) {
            mGuestDir = context.getFileStreamPath("guest");
        }
        return mGuestDir;
    }

    private static GeckoProfile getGuestProfile(Context context) {
        if (mGuestProfile == null) {
            File guestDir = getGuestDir(context);
            if (guestDir.exists()) {
                mGuestProfile = get(context, "guest", guestDir);
                mGuestProfile.mInGuestMode = true;
            }
        }

        return mGuestProfile;
    }

    public static boolean maybeCleanupGuestProfile(final Context context) {
        final GeckoProfile profile = getGuestProfile(context);

        if (profile == null) {
            return false;
        } else if (!profile.locked()) {
            profile.mInGuestMode = false;

            // If the guest dir exists, but it's unlocked, delete it
            removeGuestProfile(context);

            return true;
        }
        return false;
    }

    private static void removeGuestProfile(Context context) {
        try {
            File guestDir = getGuestDir(context);
            if (guestDir.exists()) {
                delete(guestDir);
            }
        } catch (Exception ex) {
            Log.e(LOGTAG, "Error removing guest profile", ex);
        }
    }

    public static boolean delete(File file) throws IOException {
        // Try to do a quick initial delete
        if (file.delete())
            return true;

        if (file.isDirectory()) {
            // If the quick delete failed and this is a dir, recursively delete the contents of the dir
            String files[] = file.list();
            for (String temp : files) {
                File fileDelete = new File(file, temp);
                delete(fileDelete);
            }
        }

        // Even if this is a dir, it should now be empty and delete should work
        return file.delete();
    }

    // Warning, Changing the lock file state from outside apis will cause this to become out of sync
    public boolean locked() {
        if (mLocked != LockState.UNDEFINED) {
            return mLocked == LockState.LOCKED;
        }

        // Don't use getDir() as it will create a dir if none exists
        if (mProfileDir != null && mProfileDir.exists()) {
            File lockFile = new File(mProfileDir, LOCK_FILE_NAME);
            boolean res = lockFile.exists();
            mLocked = res ? LockState.LOCKED : LockState.UNLOCKED;
        } else {
            mLocked = LockState.UNLOCKED;
        }

        return mLocked == LockState.LOCKED;
    }

    public boolean lock() {
        try {
            // If this dir doesn't exist getDir will create it for us
            File lockFile = new File(getDir(), LOCK_FILE_NAME);
            boolean result = lockFile.createNewFile();
            if (result) {
                mLocked = LockState.LOCKED;
            } else {
                mLocked = LockState.UNLOCKED;
            }
            return result;
        } catch(IOException ex) {
            Log.e(LOGTAG, "Error locking profile", ex);
        }
        mLocked = LockState.UNLOCKED;
        return false;
    }

    public boolean unlock() {
        // Don't use getDir() as it will create a dir
        if (mProfileDir == null || !mProfileDir.exists()) {
            return true;
        }

        try {
            File lockFile = new File(mProfileDir, LOCK_FILE_NAME);
            boolean result = delete(lockFile);
            if (result) {
                mLocked = LockState.UNLOCKED;
            } else {
                mLocked = LockState.LOCKED;
            }
            return result;
        } catch(IOException ex) {
            Log.e(LOGTAG, "Error unlocking profile", ex);
        }
        mLocked = LockState.LOCKED;
        return false;
    }

    private GeckoProfile(Context context, String profileName) {
        mName = profileName;
        if (sMozillaDir == null) {
            sMozillaDir = getMozillaDirectory(context);
        }
    }

    public boolean inGuestMode() {
        return mInGuestMode;
    }

    private void setDir(File dir) {
        if (dir != null && dir.exists() && dir.isDirectory()) {
            mProfileDir = dir;
        } else {
            Log.w(LOGTAG, "Requested profile directory missing.");
        }
    }

    public String getName() {
        return mName;
    }

    public synchronized File getDir() {
        forceCreate();
        return mProfileDir;
    }

    public synchronized GeckoProfile forceCreate() {
        if (mProfileDir != null) {
            return this;
        }

        try {
            // Check if a profile with this name already exists.
            File mozillaDir = ensureMozillaDirectory();
            mProfileDir = findProfileDir(mozillaDir);
            if (mProfileDir == null) {
                // otherwise create it
                mProfileDir = createProfileDir(mozillaDir);
            } else {
                Log.d(LOGTAG, "Found profile dir.");
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Error getting profile dir", ioe);
        }
        return this;
    }

    public File getFile(String aFile) {
        File f = getDir();
        if (f == null)
            return null;

        return new File(f, aFile);
    }

    /**
     * Moves the session file to the backup session file.
     *
     * sessionstore.js should hold the current session, and sessionstore.bak
     * should hold the previous session (where it is used to read the "tabs
     * from last time"). Normally, sessionstore.js is moved to sessionstore.bak
     * on a clean quit, but this doesn't happen if Fennec crashed. Thus, this
     * method should be called after a crash so sessionstore.bak correctly
     * holds the previous session.
     */
    public void moveSessionFile() {
        File sessionFile = getFile("sessionstore.js");
        if (sessionFile != null && sessionFile.exists()) {
            File sessionFileBackup = getFile("sessionstore.bak");
            sessionFile.renameTo(sessionFileBackup);
        }
    }

    /**
     * Get the string from a session file.
     *
     * The session can either be read from sessionstore.js or sessionstore.bak.
     * In general, sessionstore.js holds the current session, and
     * sessionstore.bak holds the previous session.
     *
     * @param readBackup if true, the session is read from sessionstore.bak;
     *                   otherwise, the session is read from sessionstore.js
     *
     * @return the session string
     */
    public String readSessionFile(boolean readBackup) {
        File sessionFile = getFile(readBackup ? "sessionstore.bak" : "sessionstore.js");

        try {
            if (sessionFile != null && sessionFile.exists()) {
                return readFile(sessionFile);
            }
        } catch (IOException ioe) {
            Log.e(LOGTAG, "Unable to read session file", ioe);
        }
        return null;
    }

    public String readFile(String filename) throws IOException {
        File dir = getDir();
        if (dir == null) {
            throw new IOException("No profile directory found");
        }
        File target = new File(dir, filename);
        return readFile(target);
    }

    private String readFile(File target) throws IOException {
        FileReader fr = new FileReader(target);
        try {
            StringBuilder sb = new StringBuilder();
            char[] buf = new char[8192];
            int read = fr.read(buf);
            while (read >= 0) {
                sb.append(buf, 0, read);
                read = fr.read(buf);
            }
            return sb.toString();
        } finally {
            fr.close();
        }
    }

    private boolean remove() {
        try {
            File dir = getDir();
            if (dir.exists())
                delete(dir);

            File mozillaDir = ensureMozillaDirectory();
            mProfileDir = findProfileDir(mozillaDir);
            if (mProfileDir == null) {
                return false;
            }

            INIParser parser = getProfilesINI(mozillaDir);

            Hashtable<String, INISection> sections = parser.getSections();
            for (Enumeration<INISection> e = sections.elements(); e.hasMoreElements();) {
                INISection section = e.nextElement();
                String name = section.getStringProperty("Name");

                if (name == null || !name.equals(mName))
                    continue;

                if (section.getName().startsWith("Profile")) {
                    // ok, we have stupid Profile#-named things.  Rename backwards.
                    try {
                        int sectionNumber = Integer.parseInt(section.getName().substring("Profile".length()));
                        String curSection = "Profile" + sectionNumber;
                        String nextSection = "Profile" + (sectionNumber+1);

                        sections.remove(curSection);

                        while (sections.containsKey(nextSection)) {
                            parser.renameSection(nextSection, curSection);
                            sectionNumber++;
                            
                            curSection = nextSection;
                            nextSection = "Profile" + (sectionNumber+1);
                        }
                    } catch (NumberFormatException nex) {
                        // uhm, malformed Profile thing; we can't do much.
                        Log.e(LOGTAG, "Malformed section name in profiles.ini: " + section.getName());
                        return false;
                    }
                } else {
                    // this really shouldn't be the case, but handle it anyway
                    parser.removeSection(mName);
                    return true;
                }
            }

            parser.write();
            return true;
        } catch (IOException ex) {
            Log.w(LOGTAG, "Failed to remove profile.", ex);
            return false;
        }
    }

    public static String findDefaultProfile(Context context) {
        // Have we read the default profile from the INI already?
        // Changing the default profile requires a restart, so we don't
        // need to worry about runtime changes.
        if (sDefaultProfileName != null) {
            return sDefaultProfileName;
        }

        // Open profiles.ini to find the correct path
        INIParser parser = getProfilesINI(getMozillaDirectory(context));

        for (Enumeration<INISection> e = parser.getSections().elements(); e.hasMoreElements();) {
            INISection section = e.nextElement();
            if (section.getIntProperty("Default") == 1) {
                sDefaultProfileName = section.getStringProperty("Name");
                return sDefaultProfileName;
            }
        }

        return null;
    }

    private File findProfileDir(File mozillaDir) {
        // Open profiles.ini to find the correct path
        INIParser parser = getProfilesINI(mozillaDir);

        for (Enumeration<INISection> e = parser.getSections().elements(); e.hasMoreElements();) {
            INISection section = e.nextElement();
            String name = section.getStringProperty("Name");
            if (name != null && name.equals(mName)) {
                if (section.getIntProperty("IsRelative") == 1) {
                    return new File(mozillaDir, section.getStringProperty("Path"));
                }
                return new File(section.getStringProperty("Path"));
            }
        }

        return null;
    }

    private static String saltProfileName(String name) {
        String allowedChars = "abcdefghijklmnopqrstuvwxyz0123456789";
        StringBuilder salt = new StringBuilder(16);
        for (int i = 0; i < 8; i++) {
            salt.append(allowedChars.charAt((int)(Math.random() * allowedChars.length())));
        }
        salt.append('.');
        salt.append(name);
        return salt.toString();
    }

    private File createProfileDir(File mozillaDir) throws IOException {
        INIParser parser = getProfilesINI(mozillaDir);

        // Salt the name of our requested profile
        String saltedName = saltProfileName(mName);
        File profileDir = new File(mozillaDir, saltedName);
        while (profileDir.exists()) {
            saltedName = saltProfileName(mName);
            profileDir = new File(mozillaDir, saltedName);
        }

        // Attempt to create the salted profile dir
        if (!profileDir.mkdirs()) {
            throw new IOException("Unable to create profile.");
        }
        Log.d(LOGTAG, "Created new profile dir.");

        // Now update profiles.ini
        // If this is the first time its created, we also add a General section
        // look for the first profile number that isn't taken yet
        int profileNum = 0;
        boolean isDefaultSet = false;
        INISection profileSection;
        while ((profileSection = parser.getSection("Profile" + profileNum)) != null) {
            profileNum++;
            if (profileSection.getProperty("Default") != null) {
                isDefaultSet = true;
            }
        }

        profileSection = new INISection("Profile" + profileNum);
        profileSection.setProperty("Name", mName);
        profileSection.setProperty("IsRelative", 1);
        profileSection.setProperty("Path", saltedName);

        if (parser.getSection("General") == null) {
            INISection generalSection = new INISection("General");
            generalSection.setProperty("StartWithLastProfile", 1);
            parser.addSection(generalSection);
        }

        if (!isDefaultSet && !mName.startsWith("webapp")) {
            // only set as default if this is the first non-webapp
            // profile we're creating
            profileSection.setProperty("Default", 1);
        }

        parser.addSection(profileSection);
        parser.write();

        // Write out profile creation time, mirroring the logic in nsToolkitProfileService.
        try {
            FileOutputStream stream = new FileOutputStream(profileDir.getAbsolutePath() + File.separator + "times.json");
            OutputStreamWriter writer = new OutputStreamWriter(stream, Charset.forName("UTF-8"));
            try {
                writer.append("{\"created\": " + System.currentTimeMillis() + "}\n");
            } finally {
                writer.close();
            }
        } catch (Exception e) {
            // Best-effort.
            Log.w(LOGTAG, "Couldn't write times.json.", e);
        }

        return profileDir;
    }
}
