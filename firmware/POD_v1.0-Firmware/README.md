#  nRF Connect SDK v3.0.2 Setup and Build Guide

This guide walks you through setting up the nRF development environment using **nRF Connect SDK v3.0.2**, and building a project using the `proj.conf` file and the default **nRF52840** board overlay.

---

##  Prerequisites

- A PC running Windows, macOS, or Linux
- Nordic nRF52840 on POD board
- Internet connection

---

##  1. Install nRF Connect and Toolchain

### Step 1. Install **nRF Connect for Desktop**

Download and install from:  
👉 https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-desktop

### Step 2. Install **Toolchain Manager**

Open **nRF Connect for Desktop**, install the **Toolchain Manager**.

### Step 3. Install SDK v3.0.2

1. Launch **Toolchain Manager**
2. Click **Install SDK** → select version `v3.0.2`
3. Install it to a convenient location (e.g., `C:\ncs\v3.0.2` or `~/ncs/v3.0.2`)
4. After install, click **Open Terminal** from the SDK entry to open an environment-ready shell

---

##  2. Create a New Project (use BLE Mesh example)

Use the terminal from Toolchain Manager (preloaded with environment variables):

```bash
cd ~/ncs/v3.0.2
west init -m https://github.com/nrfconnect/sdk-nrf --mr v3.0.2 my-app
cd my-app
west update
west zephyr-export
```

---

##  3. Project Structure

Ensure your project folder looks like this:

```
firmware/
├── base_project/
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── src/
│       └── main.c
```

#### Sample `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(base_project)
target_sources(app PRIVATE src/main.c)
```

#### Sample `prj.conf`:

```conf
CONFIG_GPIO=y
CONFIG_SERIAL=y
CONFIG_PRINTK=y
CONFIG_LOG=y
```

---

##  4. Build with Default nRF52840 Overlay

No custom `overlay-*.dts` file is needed if you're using the default board configuration for `nrf52840dk_nrf52840`.

From the `base_project` root:

```bash
west build -b nrf52840dk_nrf52840 app
```

This will:
- Use the `proj.conf` for configuration
- Use the default board DTS and overlay file from the SDK

### 🧪 Flash the Firmware

Plug in your DK and run:

```bash
west flash
```

---

## 📝 Notes

- You can view Kconfig options in `prj.conf` using:  
  `menuconfig` (e.g. `west build -t menuconfig`)
- If you need overlays later, place them in `app/boards/` as `nrf52840dk_nrf52840.overlay`

---

##  Troubleshooting

- ❌ **Command not found?**  
  Make sure you’re using the terminal launched from **Toolchain Manager**
  
- ❌ **Build error due to Kconfig?**  
  Run `west build -t menuconfig` to inspect misconfigurations.

- ❌ **Flash error due to nrfutil not available?**  
  Run `west flash --runner nrfjprog` to use nrfjprog instead.

---

✅ **Done!** You now have a working build setup using SDK v3.0.2 and the default nRF52840 board files.
