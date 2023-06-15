# # function to install dependencies
# install_dependency() {
#   local dependencies=("$@")

#   # function to check if a command exists
#   command_exists() {
#     command -v "$1" >/dev/null 2>&1
#   }

#   # array to hold packages that need to be installed
#   local packages_to_install=()

#   # loop through each dependency
#   for dependency in "${dependencies[@]}"; do
#     # split the dependency into command name and package name
#     IFS=":" read -r command_name package_name <<< "$dependency"

#     # if package name is not provided, use command name as the package name
#     if [ -z "$package_name" ]; then
#       package_name="$command_name"
#     fi

#     # check if the command is already installed
#     if command_exists "$command_name"; then
#       echo "$command_name is already installed. ✅"
#     else
#       echo "$command_name is not installed. Proceeding with installation..."
#       packages_to_install+=("$package_name")
#     fi
#   done

#   # update package list and install all packages at once using apt-get
#   if [ ${#packages_to_install[@]} -ne 0 ]; then
#     apt-get update >/dev/null
#     if apt-get install -y "${packages_to_install[@]}" >/dev/null; then
#       echo "All packages installed successfully. ✅"
#     else
#       echo "Failed to install some packages. ❌"
#     fi
#   fi
# }

# # Define the dependencies to install
# DEPENDENCIES=("jq" "aria2c:aria2")
# # Install the dependencies
# install_dependency "${DEPENDENCIES[@]}"