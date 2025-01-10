Building Firefox on Windows using WSL
=======================================

These steps were verified to work as of June 2024.

#. Install WSL (Windows Subsystem for Linux) on Windows by running Windows Powershell as Administrator and typing:

   .. code::

      wsl --install

#. Reboot Windows

#. Upon reboot, you should be asked to set up a user and password for Linux. If not, open Ubuntu from the Start menu.

#. Follow the :ref:`Building Firefox On Linux` instructions.

   .. note::

      For Mercurial to work, you will need to follow the instructions for bash and restart WSL.

#. Run the following commands to install dependencies needed to build and run Firefox:

   .. code::

     	sudo apt update
     	sudo apt install libgtk-3-0 libasound2 libx11-xcb-dev
