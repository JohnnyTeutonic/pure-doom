#!/bin/bash

# Script to install SDL2_image dependencies for PureDoom

echo "Checking for SDL2_image..."

# Check OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # Linux
    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        echo "Detected Debian/Ubuntu system"
        echo "Installing SDL2_image via apt..."
        sudo apt-get update
        sudo apt-get install -y libsdl2-image-dev
    elif command -v dnf &> /dev/null; then
        # Fedora
        echo "Detected Fedora system"
        echo "Installing SDL2_image via dnf..."
        sudo dnf install -y SDL2_image-devel
    elif command -v pacman &> /dev/null; then
        # Arch
        echo "Detected Arch Linux system"
        echo "Installing SDL2_image via pacman..."
        sudo pacman -S --noconfirm sdl2_image
    else
        echo "Could not determine package manager. Please install SDL2_image manually."
    fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    if command -v brew &> /dev/null; then
        echo "Detected macOS with Homebrew"
        echo "Installing SDL2_image via brew..."
        brew install sdl2_image
    else
        echo "Homebrew not found. Please install it or install SDL2_image manually."
        echo "Install Homebrew with: /bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    fi
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    # Windows with MSYS/MinGW
    echo "Detected Windows system"
    echo "On Windows, please install SDL2_image manually:"
    echo "1. Download SDL2_image development libraries from https://www.libsdl.org/projects/SDL_image/"
    echo "2. Extract to a location on your system"
    echo "3. Set the environment variable SDL2_DIR to point to the SDL2 installation directory"
    echo "4. Add the SDL2 bin directory to your PATH"
else
    echo "Unsupported OS: $OSTYPE"
    echo "Please install SDL2_image manually."
fi

echo "Dependency check complete!" 