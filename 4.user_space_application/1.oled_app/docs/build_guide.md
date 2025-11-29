# Hướng dẫn build và deploy

# build:
# $ source /home/adonisking/Learning_Linux/3.Device_driver/SDK/environment-setup-cortexa72-poky-linux
# $ cd /home/adonisking/Learning_Linux/4.user_space_application/1.oled_app/build
# cmake -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE ..
# To see the target build:
# $ cmake --build . --target help
# Example build test oled_write_pixel:
# $ cmake --build . --target oled_write_pixel