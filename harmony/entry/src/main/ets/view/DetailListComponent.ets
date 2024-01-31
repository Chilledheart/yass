import { CommonConstants } from '../constants/CommonConstants';
import { YassDataItem } from '../viewmodel/YassDataItem';
import yass from 'libyass.so';

/**
 * This is the general detail page component that can add multiple items to the list.
 */
@Component
export struct DetailListComponent {
  @Link private dataItem: YassDataItem;

  build() {
    Column() {
      Row() {
        Text($r('app.string.server_host'))
          .fontSize($r('app.float.name_text_size'))
          .fontColor($r('app.color.text'))
        Blank()
        TextInput({text: this.dataItem.serverHost})
          .width(CommonConstants.DETAIL_INPUT_WIDTH_PERCENT)
          .fontSize($r('app.float.app_info_sub_title_size'))
          .fontColor($r('app.color.text_opacity'))
          .onChange((value: string) => {
            this.dataItem.serverHost = value;
          })
      }
      .width(CommonConstants.ROW_WIDTH_PERCENT)
      .height($r('app.float.list_height'))
      Row() {
        Text($r('app.string.server_sni'))
          .fontSize($r('app.float.name_text_size'))
          .fontColor($r('app.color.text'))
        Blank()
        TextInput({text: this.dataItem.serverSNI})
          .width(CommonConstants.DETAIL_INPUT_WIDTH_PERCENT)
          .fontSize($r('app.float.app_info_sub_title_size'))
          .fontColor($r('app.color.text_opacity'))
          .onChange((value: string) => {
            this.dataItem.serverSNI = value;
          })
      }
      .width(CommonConstants.ROW_WIDTH_PERCENT)
      .height($r('app.float.list_height'))
      Row() {
        Text($r('app.string.server_port'))
          .fontSize($r('app.float.name_text_size'))
          .fontColor($r('app.color.text'))
        Blank()
        TextInput({text: this.dataItem.serverPort.toString()})
          .width(CommonConstants.DETAIL_INPUT_WIDTH_PERCENT)
          .type(InputType.Number)
          .fontSize($r('app.float.app_info_sub_title_size'))
          .fontColor($r('app.color.text_opacity'))
          .onChange((value: string) => {
            this.dataItem.serverPort = Number(value);
          })
      }
      .width(CommonConstants.ROW_WIDTH_PERCENT)
      .height($r('app.float.list_height'))
      Row() {
        Text($r('app.string.username'))
          .fontSize($r('app.float.name_text_size'))
          .fontColor($r('app.color.text'))
        Blank()
        TextInput({text: this.dataItem.username})
          .width(CommonConstants.DETAIL_INPUT_WIDTH_PERCENT)
          .fontSize($r('app.float.app_info_sub_title_size'))
          .fontColor($r('app.color.text_opacity'))
          .onChange((value: string) => {
            this.dataItem.username = value;
          })
      }
      .width(CommonConstants.ROW_WIDTH_PERCENT)
      .height($r('app.float.list_height'))
      Row() {
        Text($r('app.string.password'))
          .fontSize($r('app.float.name_text_size'))
          .fontColor($r('app.color.text'))
        Blank()
        TextInput({text: this.dataItem.password})
          .width(CommonConstants.DETAIL_INPUT_WIDTH_PERCENT)
          .type(InputType.Password)
          .fontSize($r('app.float.app_info_sub_title_size'))
          .fontColor($r('app.color.text_opacity'))
          .onChange((value: string) => {
            this.dataItem.password = value;
          })
      }
      .width(CommonConstants.ROW_WIDTH_PERCENT)
      .height($r('app.float.list_height'))
      Row() {
        Text($r('app.string.cipher'))
          .fontSize($r('app.float.name_text_size'))
          .fontColor($r('app.color.text'))
        Blank()
        TextPicker({range: yass.getCipherStrings(), selected: yass.getCipherStrings().indexOf(this.dataItem.cipher)})
          .width(CommonConstants.DETAIL_INPUT_WIDTH_PERCENT)
          .height($r('app.float.list_height'))
          .onChange((value: string, index: number) => {
            this.dataItem.cipher = value;
          })
      }
      .width(CommonConstants.ROW_WIDTH_PERCENT)
      .height($r('app.float.list_height'))
      Row() {
        Text($r('app.string.timeout'))
          .fontSize($r('app.float.name_text_size'))
          .fontColor($r('app.color.text'))
        Blank()
        TextInput({text: this.dataItem.timeout.toString()})
          .width(CommonConstants.DETAIL_INPUT_WIDTH_PERCENT)
          .type(InputType.Number)
          .fontSize($r('app.float.app_info_sub_title_size'))
          .fontColor($r('app.color.text_opacity'))
          .onChange((value: string) => {
            this.dataItem.timeout = Number(value);
          })
      }
      .width(CommonConstants.ROW_WIDTH_PERCENT)
      .height($r('app.float.list_height'))
    }
    .width(CommonConstants.DETAIL_LIST_WIDTH_PERCENT)
    .padding({
      top: $r('app.float.list_padding_top'),
      bottom: $r('app.float.list_padding_bottom'),
      left: $r('app.float.list_padding_left'),
      right: $r('app.float.list_padding_right')
    })
    .margin({ top: $r('app.float.name_text_padding_top') })
    .backgroundColor($r('app.color.start_window_background'))
    .borderRadius($r('app.float.detail_list_radius'))
  }
}
