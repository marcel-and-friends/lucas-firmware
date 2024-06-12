from distutils.dir_util import copy_tree

import pioutil
if pioutil.is_pio_build():
    Import("env")

    src_dir = f"{env['PROJECT_DIR']}/buildroot/share/PlatformIO/variants/MARLIN_F4x7Vx"
    dst_dir = f"{env['PROJECT_PACKAGES_DIR']}/framework-arduinoststm32/variants/MARLIN_F4x7Vx"

    copy_tree(src_dir, dst_dir, update = True)

    print(f"LUCAS: Copiado de '{src_dir}' para '{dst_dir}'.")
