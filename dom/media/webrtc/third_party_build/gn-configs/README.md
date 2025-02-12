# Generate new gn json files and moz.build files for building libwebrtc in our tree

/!\ This is only supported on Linux and macOS. If you are on Windows, you can run
the script under [WSL](https://docs.microsoft.com/en-us/windows/wsl/install).

1. The script should be run from the top directory of our firefox tree.

   ```
   ./mach python build/gn_processor.py dom/media/webrtc/third_party_build/gn-configs/webrtc.json
   ```

2. Checkin all the generated/modified files and try your build!

# Adding new configurations to the build

Edit the `main` function in the `build/gn_processor.py` file.
