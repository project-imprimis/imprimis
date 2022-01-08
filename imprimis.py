# To detect operating sytem type.
from sys import platform
# To install cppyy module if it does not exist.
import os
# To join paths in a cross-platform way.
from os import path
# To access arguments passed to the script.
import sys

try:
  # Try to import cppyy to call C++ from Python.
  import cppyy

except ImportError:
  # If module could not be imported, prompt user to install it.
  intall = ('The cppyy module is required. Install? [y/n] ')

  # Convert to lowercase.
  install = install.lower()

  # If install is 'y' or 'yes':
  if install in set(['', 'y','ye','yes']):

    # Install cppyy.
    print('Installing cppyy.')
    if platform == 'linux':
        # If Linux, use ~/.imprimis
        installation_message = os.system('sudo python3 -m pip install cppyy')

    elif platform == 'win32':
        # If Windows, use $HOME\My Games\Imprimis
        installation_message = os.system('python -m pip install cppyy')

    try:
      # Try to import cppyy after it has been isntalled.
      import cppyy

    except ImportError:
      # Throw error message if installation failed.
      raise ModuleNotFoundError('Could not install cppyy.')

# Get home directory.
def get_home_directory() -> str:
  ''' Return the game's home directory. '''

  if platform == 'linux':
      # If Linux, use ~/.imprimis
      return str(path.join(path.expanduser('~'), '.imprimis'))

  elif platform == 'win32':
      # If Windows, use $HOME\My Games\Imprimis
      return str(path.join(path.expanduser('~'), 'My Games', 'Imprimis'))

  # Return an error message for any other operating systems.
  raise RuntimeError('Only Windows and Linux operating systems are supported.')

def get_imprimis_lib_path() -> str:
  ''' Return path to imprimis library. '''

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
    raise RuntimeError('Only Windows and Linux operating systems are supported.')

  if not path.isfile(imprimis_lib_path):
    print('''Your platform does not have a pre-compiled imprimis.so library.
    Please follow the following steps to build a native client:
    1) Type 'make' to co compile the imprimis.so library.
    2) Ensure you have the engine binary at /usr/lib/libprimis.so.
    3) Ensure you have the SDL2, SDL2-image, SDL2-mixer, and OpenGL libraries installed.
    4) If the build succeeds, run this script again.''')

    # Return an error.
    raise RuntimeError(f'Unable to load {imprimis_lib_path}.')

  # Return the path.
  return imprimis_lib_path


def get_file_path(name: str = "main.cpp") -> str:
  ''' Return path to main.cpp, or any file in the game folder. '''

  # Get file path.
  file_path = str(path.join(path.curdir, 'game', name))

  # Check that the file exists.
  if path.isfile(file_path):
    return file_path

  # Else the file does not exist.
  raise RuntimeError(f'The file {file_path} does not eixst.')

# Load shared library.
cppyy.load_library(get_imprimis_lib_path())

# Load the main function.
cppyy.include(get_file_path('main.cpp'))

# Print the settings folder.
print('Game folder:', get_home_directory())

# Sart the executable with the given parameters: The home directory (-u) and log
# file name (-g). The last argument sys.argv[1:] represents all the arguments
# passed when calling the script. For example in `script.py arg1 arg2 arg3`, the
# symbol sys.argv[1:] would represent arg1, arg2, and arg3. The 0th value is the
# name of the script, so sys.arg[0] would be `script.py`.
# If no log file name is specified, the logs will be printed directly on the
# terminal. If a log file name is given, the log file will be saved in the
# current directory.
parameters = ['-u' + get_home_directory(), '-glog.txt'] + sys.argv[0:]

# Call the main function and pass the arguments.
cppyy.gbl.main(len(parameters), parameters)