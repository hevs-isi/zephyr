west init -m https://github.com/hevs-isi/zephyr watermon-mcu
cd watermon-cpu
west update

cd zephyr
git fetch --all
git checkout --track origin/watermon

source zephyr-env.sh
cd app/

west build -b nucleo
west build -b nucleo_l476rg
west flash
