# Exercise Setup Guide

## Prerequisites

Ensure you have the following installed:

* **Git**
* **CMake** version 3.24 or higher
* **C++ compiler** that supports C++17

## Installation

This project utilizes CMake and includes the CGV framework as a submodule. Follow these steps to set up the repository and its submodules:

### Cloning the Repository

To clone the repository along with its submodules, use the following command:

```bash
git clone --recurse-submodules https://bitbucket.org/cgvtud/scivis
```

If you've already cloned the repository without its submodules, initialize them with:

```bash
git submodule update --init --recursive
```

## Building the Exercises

The project builds are managed using CMake. Start by navigating to the project's directory:

```bash
cd scivis
```

### Cross-Platform: Visual Studio Code

Visual Studio Code can be used on both Windows and Linux. When you open the project, Visual Studio Code will recommend necessary extensions.

Use the CMake extension to configure and build the project. Configuration should occur automatically upon opening, but you can adjust settings as needed and configure again. A launch configuration will be automatically generated to allow running and debugging exercises.

For building:

* On Windows, the CodeLLDB extension is used.
* On Linux, either CodeLLDB or cppdbg may be used.

To build, press `F1` and type `CMake: Configure` and `CMake: Build`, or use the buttons in the bottom left corner. Ensure the build target is set to `all`.

*Note: It is also possible to use devcontainers with docker for advanced users, for more see [here](#markdown-header-using-devcontainers-advanced-users).*

Alternatively, you may follow the following instructions to build from command line on Windows or Linux:

### Building on Windows

Using Visual Studio 2022 (recommended and tested), initialize and generate the solution with:

```bash
cmake -B cmake-build -G "Visual Studio 17 2022"
```

For other supported versions (e.g., Visual Studio 2019), modify the generator flag appropriately:

```bash
cmake -B cmake-build -G "Visual Studio 16 2019"
```

Be advised that not all versions of Visual Studio are supported. Tested versions are 2022 and 2019.

--------

Once configured, open `SciVis.sln` in the `cmake-build` directory. 

On the right side (in the Solution Explorer), you will see a list of projects. Right-click on the project you want to build and select 'Set as StartUp Project'. Then either press `F5` or click on the green play button to build and run the project.

If you want to change the project you are working on, you can once again right-click on the project and select 'Set as StartUp Project'.

![Video showcasing selecting the project to build](https://i.imgur.com/ZYFNfJV.gif)

### Building on Linux

Build the project with:

```bash
cmake -B cmake-build
cmake --build cmake-build
```

To run an exercise, navigate to the build directory and execute:

```bash
./cmake-build/run_intro.sh
```

## Project Structure

This structure applies to individual exercise READMEs. Modify the path to the specific exercise folder as necessary:

### Folder Structure

Each exercise operates independently within the project directory:

```
scivis
    |- cmake-build   --> All solutions, libraries, and binaries are built here.
    |- 0-intro       --> Source code for the introductory exercise
    |- 1-stereo
    |- ...
    |- cgv           --> CGV framework repository as a submodule
```

## Additional Information

Building with other systems like the *.pj build system is possible. For more details, refer to the [CGV framework official documentation](https://sgumhold.github.io/cgv/install.html).

### Using Devcontainers (Advanced Users)
Ensure you have Docker installed. This setup includes a preconfigured environment using a devcontainer, ideal if you are familiar with Docker and devcontainers.

Note: Exercises require a functioning X11 server for visual output.

* **Windows**: Use tools like VcXSrv or MobaXTerm. Configure these tools to allow more permissive connections.
* **Linux**: Allow connections to your X11 server with:

  ```bash
  xhost local:root
  ```

Adjust the devcontainer configuration file according to your OS.
