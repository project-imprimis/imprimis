# To detect operating sytem type, and to access arguments passed to the script.
from sys import platform, argv
# To install cppyy module if it does not exist.
from subprocess import run
# To join paths in a cross-platform way.
from os import path
# TODO: Create unistaller.

def install_required_modules():
    ''' Prompt the user to install `cppyy`. '''
    # Prompt user to install module.
    install = input('The cppyy module is required. Install (y/n)? ')

    # Convert to lowercase.
    install = install.lower()

    # If install is 'y' or 'yes':
    if install in set(['', 'y','ye','yes']):

        if platform == 'linux':
            # If Linux, use ~/.imprimis
             run('python3 -m pip install cppyy', shell=True)

        elif platform == 'win32':
            # If Windows, use $HOME\My Games\Imprimis
            run('python -m pip install cppyy', shell=True)

        else:
            # Return an error for other operating systems.
            raise RuntimeError('Your operating system is not supported.')

        # Run the script after installing the dependencies. Note that
        # sys.argv[0] is the name of the script, which is stored as an argument.
        # Example: if we run `python run.py`, then sys.argv[0] equals `run.py`.
        run(f'python {argv[0]}', shell=True)

def compile_imprimis_library():
    '''
    Check that the Imprimis shared library dependencies are present, and then
    compile the shared Imprimis library. Only works on Linux.
    '''
    #TODO: Add Windows support for automatic install.

    # libsdl2, libsdl2-image, libsdl2-mixer, libsdl2-ttf
    # Define Makefile path.
    makefile_path = str(path.join(path.curdir, 'Makefile'))

    if platform == 'linux':
        # If Linux, build the Makefile.
        # shell=True: No need to split commands.
        # capture_output=True: Captures stdout and stderr.
        # text=True: Stores sdtout and stderr as a string, not a byte array.

        # Build the Makefile, and capture any error codes (0 means success).
        if run('make -j3', shell=True).returncode != 0:
            # If build fails, show error message.
            print('\nYour platform faild to compile the imprimis.so library.')
            print('Please follow the following steps to build a native client:')
            print('    1) Ensure you have the SDL2, SDL2-image, SDL2-mixer, and OpenGL')
            print('       libraries installed.')
            print('    2) Ensure you have the engine binary at /usr/lib/libprimis.so.')
            # TODO: Prompt the user to install SDL2, SDL2-image, SDL2-mixer, and OpenGL.
            # install_missing_libraries()
            # TODO: Prompt the user to install libprimis.so.
            # install_libprimis_library()

    elif platform == 'win32':
        # If Windows, build Makefile.
        # TODO: Find a way to build Imprimis on windows through the terminal.
        # run('?????????', shell=True)
        print('Use Visual Studio Code to build the game library imprimis.so.')

    else:
        # Return an error message for any other operating systems.
        raise RuntimeError('Your operating system is not supported.')