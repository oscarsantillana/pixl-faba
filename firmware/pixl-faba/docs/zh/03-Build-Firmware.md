# 构建

## 使用Github Actions进行构建

您可以从Github Actions下载最新的构建开发版本。

https://github.com/solosky/pixl.js/actions


## 使用定制的Docker镜像进行构建

您可以使用定制的Docker镜像构建固件。

```
# 创建容器
docker run -it --rm solosky/nrf52-sdk:latest

# 下载代码
root@b10d54636088:/builds# git clone https://github.com/solosky/pixl.js
root@b10d54636088:/builds# cd pixl.js
root@b10d54636088:/builds/pixl.js# git submodule update --init --recursive

# 构建LCD固件
root@b10d54636088:/builds/pixl.js# cd fw && make all BOARD=LCD RELEASE=1

# 构建OLED固件
root@b10d54636088:/builds/pixl.js# cd fw && make all BOARD=OLED RELEASE=1

# 构建OLED按键板固件（四键：左/右/确定/返回，返回键 P0.20）
root@b10d54636088:/builds/pixl.js# cd fw && make all BOARD=KEYPAD RELEASE=1
```

构建好的固件在 fw/_build/pixjs_all.hex，ota package is fw/_build/pixjs_ota_vXXXX.zip

## AmiiboTool / iNFC 四键设备

带独立返回键（P0.20）的 AmiiboTool、iNFC 等改版硬件**必须**使用 `BOARD=KEYPAD` 固件，**不要**使用标准三键 `BOARD=OLED` 包。

- GitHub Actions：下载 **`amiibotool-fw-keypad`** 产物（`amiibotool` 分支的 `amiibotool-fw` 工作流）
- 本地构建：`make all BOARD=KEYPAD RELEASE=1`（OLED/KEYPAD 板会自动为 bootloader 启用 SH1106）
- 首次刷写或更换板型后，请用 **`pixljs_all.hex` 全量线刷**（含 bootloader），不要仅 OTA 更新 app
- 可在 **设置 → 版本** 页核对 Git 分支与 build 信息
- KEYPAD 固件为节省 FLASH 空间未启用 **Player** 与 **游戏**；Amiibo 相关应用保持启用
