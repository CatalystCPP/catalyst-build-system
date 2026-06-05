#! /usr/bin/env bash

# if include/extern/CLI11.hpp doesn't exist, fetch it from the web
if [ ! -f include/extern/CLI11.hpp ]; then
    echo "Fetching CLI11.hpp from the web..."
    mkdir -p include/extern
    wget https://github.com/CLIUtils/CLI11/releases/download/v2.6.2/CLI11.hpp -O include/extern/CLI11.hpp
else
    # Green Text
    printf "\033[0;32mCLI11.hpp found\033[0m\n"
fi

