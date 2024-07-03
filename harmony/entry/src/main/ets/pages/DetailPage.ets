// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

import router from '@ohos.router';
import { DetailListComponent } from '../view/DetailListComponent';
import { CommonConstants } from '../constants/CommonConstants';
import { YassDataItem } from '../viewmodel/YassDataItem';
import YassViewModel from '../viewmodel/YassViewModel';
import hilog from '@ohos.hilog';
import yass from 'libyass.so';
// import wifiManager from '@ohos.wifiManager';
import wifi from '@ohos.wifi';
import UIAbility from '@ohos.app.ability.UIAbility';

/**
 * Detail page. Click the item on about page to jump to the detail page.
 */
enum StartState {
  STOPPED,
  STOPPING,
  STARTED,
  STARTING
}

@Entry
@Component
struct DetailPage {
  private titleParam: Resource = $r('app.string.title_name');
  @State private dataParam: YassDataItem = new YassDataItem;

  private state: StartState = StartState.STOPPED;
  @State private status: Resource = $r('app.string.status_stopped', 'localhost', 0);
  @State private wifi_status: Resource = $r('app.string.current_ip', '0.0.0.0', 0);
  @State private start_btn_status: boolean = true;
  @State private stop_btn_status: boolean = false;

  private timer_id: number;

  aboutToAppear() {
    this.titleParam = $r('app.string.title_name');
    this.dataParam = YassViewModel.getYassData();
    this.status = $r('app.string.status_stopped', this.dataParam.serverHost, this.dataParam.serverPort);
    this.wifi_status = $r('app.string.current_ip', this.getWifiAddress(), 0)
    this.start_btn_status = true;
    this.stop_btn_status = false;
  }

  build() {
    Column() {
      this.Title()

      GridRow({
        columns: {
          sm: CommonConstants.GRID_ROW_SM,
          md: CommonConstants.GRID_ROW_MD,
          lg: CommonConstants.GRID_ROW_LG
        },
        gutter: { x: CommonConstants.GUTTER_X },
        breakpoints: { value: CommonConstants.BREAK_POINT }
      }) {
        GridCol({
          span: {
            sm: CommonConstants.SPAN_SM,
            md: CommonConstants.SPAN_MD,
            lg: CommonConstants.SPAN_LG
          },
          offset: {
            md: CommonConstants.OFFSET_MD,
            lg: CommonConstants.OFFSET_LG
          }
        }) {
          DetailListComponent({ dataItem: $dataParam })
        }
      }

      .margin({
        left: $r('app.float.grid_row_margin_left'),
        right: $r('app.float.grid_row_margin_right')
      })
      Row() {
        Text(this.status)
          .width(CommonConstants.DETAIL_LIST_WIDTH_PERCENT)
          .fontSize($r('app.float.name_text_size'))
          .fontColor($r('app.color.text'))
      }
      .margin({
        left: $r('app.float.grid_row_margin_left'),
        right: $r('app.float.grid_row_margin_right')
      })
      Row() {
        Text(this.wifi_status)
          .width(CommonConstants.DETAIL_LIST_WIDTH_PERCENT)
          .fontSize($r('app.float.name_text_size'))
          .fontColor($r('app.color.text'))
      }
      .margin({
        left: $r('app.float.grid_row_margin_left'),
        right: $r('app.float.grid_row_margin_right')
      })
      Row() {
        Button($r('app.string.start_button'), { type: ButtonType.Capsule,
          stateEffect: this.start_btn_status })
          .width(CommonConstants.DETAIL_INPUT_WIDTH_PERCENT)
          .onClick((event: ClickEvent) => {
          if (this.state == StartState.STOPPED) {
            this.onStartClicked();
          }
        })
        Blank()
        Button($r('app.string.stop_button'), { type: ButtonType.Capsule,
          stateEffect: this.stop_btn_status })
          .width(CommonConstants.DETAIL_INPUT_WIDTH_PERCENT)
          .onClick((event: ClickEvent) => {
          if (this.state == StartState.STARTED) {
            this.onStopClicked();
          }
        })
      }
      .margin({
        left: $r('app.float.grid_row_margin_left'),
        right: $r('app.float.grid_row_margin_right')
      })
    }
    .width(CommonConstants.DETAIL_COLUMN_WIDTH_PERCENT)
    .height(CommonConstants.DETAIL_COLUMN_HEIGHT_PERCENT)
    .backgroundColor($r('app.color.page_background'))

  }

   @Builder Title() {
    Row() {
      Image($r('app.media.ic_back'))
        .width($r('app.float.image_size'))
        .height($r('app.float.image_size'))
        .margin({
          left: $r('app.float.image_margin_left'),
          right: $r('app.float.image_margin_right')
        })
        .onClick(() => {
          router.back();
        })

      Text(this.titleParam)
        .fontColor($r('app.color.title'))
        .fontSize($r('app.float.title_text_font_size'))
    }
    .width(CommonConstants.FULL_WIDTH_PERCENT)
    .height($r('app.float.title_height'))
  }

  onStartClicked() {
    this.state = StartState.STARTING;
    this.status = $r('app.string.status_starting');
    this.start_btn_status = false;
    this.stop_btn_status = false;

    let err_msg = yass.saveConfig(this.dataParam.serverHost, this.dataParam.serverSNI, this.dataParam.serverPort.toString(),
        this.dataParam.username, this.dataParam.password, this.dataParam.cipher, this.dataParam.dohUrl, this.dataParam.dotHost,
        this.dataParam.limitRate, this.dataParam.timeout.toString());
    if (err_msg != '') {
      this.onStartFailedWithMsg(err_msg);
      return;
    }

    yass.startWorker((err_msg: string, port: number) => {
      hilog.info(0x0000, 'yass', 'started %s', err_msg);
      if (err_msg == '') {
        this.state = StartState.STARTED;
        this.wifi_status = $r('app.string.current_ip', this.getWifiAddress(), port);
        this.start_btn_status = false;
        this.stop_btn_status = true;
        this.onUpdateTransferRate();
      } else {
        this.onStartFailedWithMsg(err_msg);
      }
    })
  }

  onUpdateTransferRate() {
    let rates = yass.getTransferRate();
    this.status = $r('app.string.status_started_with_rate', rates[0], rates[1]);
    this.timer_id = setTimeout(() => {
      this.onUpdateTransferRate();
    }, 1000);
  }

  onStartFailedWithMsg(err_msg) {
    this.state = StartState.STOPPED;
    this.status = $r('app.string.status_started_with_error_msg', err_msg);
    this.start_btn_status = true;
    this.stop_btn_status = false;
    AlertDialog.show({ message: $r('app.string.status_started_with_error_msg', err_msg) });
  }

  onStopClicked() {
    this.state = StartState.STOPPING;
    this.status = $r('app.string.status_stopping');
    this.start_btn_status = false;
    this.stop_btn_status = false;
    clearTimeout(this.timer_id);
    yass.stopWorker(() => {
      hilog.info(0x0000, 'yass', 'stopped %d');
      this.state = StartState.STOPPED;
      this.status = $r('app.string.status_stopped', this.dataParam.serverHost, this.dataParam.serverPort);
      this.start_btn_status = true;
      this.stop_btn_status = false;
    })
  }

  getWifiAddress() {
    if (wifi.isConnected()) {
      return wifi.getIpInfo().ipAddress.toString();
    }
    /* FIXME crash on emulator
    if (wifiManager.isConnected()) {
      return wifiManager.getIpInfo().ipAddress.toString();
    }
    */
    return "0.0.0.0";
  }
}
