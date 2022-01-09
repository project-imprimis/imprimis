# To join paths in a cross-platform way.
from os import path
# To install missing Python modules.
import dependency_installer
# To detect operating sytem type, and to access arguments passed to the script.
from sys import platform, argv

try:
    # Try to import cppyy to call C++ from Python.
    from cppyy import add_include_path, load_library, cppdef, gbl

except ImportError:
    # If the module could not be imported, prompt user to install it.
    dependency_installer.install_required_modules()

# Get home directory.
def get_home_directory() -> str:
    '''Return the game's home directory.'''

    if platform == 'linux':
        # If Linux, use ~/.imprimis
        return str(path.join(path.expanduser('~'), '.imprimis'))

    elif platform == 'win32':
        # If Windows, use $HOME\My Games\Imprimis
        return str(path.join(path.expanduser('~'), 'My Games', 'Imprimis'))

    # Return an error message for any other operating systems.
    raise RuntimeError('Your operating system is not supported.')

def get_imprimis_lib_path() -> str:
    '''Return path to imprimis library.'''

    # Define library name.
    imprimis = 'imprimis'

    # Define path.
    imprimis_lib_path = ""

    if platform == 'linux':
        # If Linux, use ./imprimis.so
        imprimis_lib_path = str(path.join(path.curdir, f'{imprimis}.so'))

    elif platform == 'win32':
        # If Windows, use .\bin64\imprimis.lib
        imprimis_lib_path = str(path.join(path.curdir, 'bin64', f'{imprimis}.lib'))

    else:
        # Return an error message for any other operating systems.
        raise RuntimeError('Your operating system is not supported.')

    # Prompt the user to compile Imprimis library if it does not exist.
    if not path.isfile(imprimis_lib_path):
        dependency_installer.compile_imprimis_library()

    # Return the path.
    return imprimis_lib_path

def get_file_path(name: str = "main.cpp") -> str:
    '''Return path to main.cpp, or any file in the game folder.'''

    # Get file path.
    file_path = str(path.join(path.curdir, 'game', name))

    # Check that the file exists.
    if path.isfile(file_path):
        return file_path

    # Else the file does not exist.
    raise RuntimeError(f'The file {file_path} does not eixst.')

def main():
    '''Run Imprimis in Windows or Linux.'''
    # Specify the location of the SDL.h shared library.
    if platform == "linux":
        # This solution takes less than 0.1 seconds, but isn't portable.
        # The header is already located in the shared library, but cppyy needs
        # to know its location. Therefore it is faster to include the path, than
        # to include the header.
        add_include_path('/usr/include/SDL2')
        # This solution takes about 0.4 seconds, but is portable.
        # Including the header is much slower than including the path.
        # include('SDL2/SDL.h')

    elif platform == "win32":
        # TODO: Test if this is one of these headers is needed in windows. It
        # may be possible to specify the header path, as it faster for cppyy.
        # add_include_path('C:\\..some path..\\SDL2\\SDL.h')
        # include('SDL.h')
        # include('SDL2\SDL.h')
        pass

    # Load the Imprimis shared library.
    load_library(get_imprimis_lib_path())

    # Sart the executable with the given parameters: The home directory (-u) and log
    # file name (-g). The last argument sys.argv[1:] represents all the arguments
    # passed when calling the script. For example in `script.py arg1 arg2 arg3`, the
    # symbol sys.argv[1:] would represent arg1, arg2, and arg3. The 0th value is the
    # name of the script, so sys.arg[0] would be `script.py`.
    # If no log file name is specified, the logs will be printed directly on the
    # terminal. If a log file name is given, the log file will be saved in the
    # current directory.
    parameters = ['-u' + get_home_directory(), '-glog.txt'] + argv[0:]

    # Since main.cpp does not have a header file, define the signature of the
    # main function.
    cppdef("int main(int, char**);")

    # Call the main function and pass the arguments.
    gbl.main(len(parameters), parameters)

if __name__ == '__main__':
    main()