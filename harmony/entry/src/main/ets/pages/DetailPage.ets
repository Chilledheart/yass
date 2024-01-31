import router from '@ohos.router';
import { DetailListComponent } from '../view/DetailListComponent';
import { CommonConstants } from '../constants/CommonConstants';
import { DataItem } from '../viewmodel/DataItem';
import AboutViewModel from '../viewmodel/AboutViewModel';
import hilog from '@ohos.hilog';
import yass from 'libyass.so';

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
  private dataParam: Array<DataItem>;

  private state: StartState = StartState.STOPPED;

  aboutToAppear() {
    let appInfo: DataItem[] = AboutViewModel.getYassInfo()
    this.titleParam = appInfo[0].title
    this.dataParam = appInfo
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
          DetailListComponent({ detailList: this.dataParam })
        }
      }

      .margin({
        left: $r('app.float.grid_row_margin_left'),
        right: $r('app.float.grid_row_margin_right')
      })
      Row() {
        Button($r('app.string.start_button'), { type: ButtonType.Capsule,
          stateEffect: this.state == StartState.STOPPED }).onClick((event: ClickEvent) => {
          if (this.state == StartState.STOPPED) {
            this.onStartClicked();
          }
        })
        Blank()
        Button($r('app.string.stop_button'), { type: ButtonType.Capsule,
          stateEffect:  this.state == StartState.STARTED }).onClick((event: ClickEvent) => {
          if (this.state == StartState.STARTED) {
            this.onStopClicked();
          }
        })
      }
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
    yass.startWorker((err_msg: string) => {
      hilog.info(0x0000, 'yass', 'started %s', err_msg);
      if (err_msg == '') {
        this.state = StartState.STARTED;
        AlertDialog.show({ message: 'The start is successful' })
      } else {
        this.state = StartState.STOPPED;
        AlertDialog.show({ message: 'The start failed: ' + err_msg })
      }
    })
  }

  onStopClicked() {
    this.state = StartState.STOPPING;
    yass.stopWorker(() => {
      hilog.info(0x0000, 'yass', 'stopped %d');
      this.state = StartState.STOPPED;
      AlertDialog.show({ message: 'The stop is successful' })
    })
  }
}