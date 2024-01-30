/**
 * Common constants for all features.
 */
export class CommonConstants {
  /**
   * The number of columns for SM device.
   */
  static readonly GRID_ROW_SM: number = 4;

  /**
   * The number of columns for MD device.
   */
  static readonly GRID_ROW_MD: number = 8;

  /**
   * The number of columns for LG device.
   */
  static readonly GRID_ROW_LG: number = 12;

  /**
   * The gutter of x direction device.
   */
  static readonly GUTTER_X: number = vp2px(12);

  /**
   * The span of page for SM device.
   */
  static readonly SPAN_SM: number = 4;

  /**
   * The span of page for MD device.
   */
  static readonly SPAN_MD: number = 6;

  /**
   * The span of page for LG device.
   */
  static readonly SPAN_LG: number = 8;

  /**
   * The offset of page for SM device.
   */
  static readonly OFFSET_SM: number = 1;

  /**
   * The offset of page for MD device.
   */
  static readonly OFFSET_MD: number = 1;

  /**
   * The offset of page for LG device.
   */
  static readonly OFFSET_LG: number = 2;

  /**
   * The weight of row for device.
   */
  static readonly ROW_WEIGHT: number = 4;

  /**
  * The weight of row for LG device.
  */
  static readonly ROW_LG_WEIGHT: number = 5;

  /**
   * The weight of flex for device.
   */
  static readonly FLEX_WEIGHT: number = 6;

  /**
   * The weight of flex for LG device.
   */
  static readonly FLEX_LG_WEIGHT: number = 5;

  /**
   * The break point for page.
   */
  static readonly BREAK_POINT: Array<string> = ['320vp', '600vp', '840vp'];

  /**
   * The break point of lg.
   */
  static readonly BREAK_POINT_LG: string = 'lg';

  /**
   * The break point of sm.
   */
  static readonly BREAK_POINT_SM: string = 'sm';

  /**
   * Font weight.
   */
  static readonly FONT_WEIGHT: number = 500;

  /**
   * Scroll weight.
   */
  static readonly SCROLL_WEIGHT: number = 1;

  /**
   * Application version example.
   */
  static readonly APP_VERSION: string = '1.0.0';

  /**
   * Detail page url.
   */
  static readonly DETAIL_PAGE: string = 'pages/DetailPage';

  /**
   * Agreement page url.
   */
  static readonly AGREEMENT_PAGE: string = 'pages/AgreementPage';

  /**
   * Privacy page url.
   */
  static readonly PRIVACY_PAGE: string = 'pages/PrivacyPage';

  /**
   * About us item id.
   */
  static readonly ABOUT_US_ID: number = 1;

  /**
   * Contact item id.
   */
  static readonly CONTACT_ID: number = 2;

  /**
   * Detail page default content row.
   */
  static readonly DETAIL_DEFAULT_ROW: number = 2;

  /**
   * This is key param of data.
   */
  static readonly KEY_PARAM_DATA: string = 'data';

  /**
   * This is key param of title.
   */
  static readonly KEY_PARAM_TITLE: string = 'title';

  /**
   * Detail column title text width percentage setting.
   */
  static readonly DETAIL_COLUMN_WIDTH_PERCENT: string = '100%';

  /**
   * Detail column height percentage setting.
   */
  static readonly DETAIL_COLUMN_HEIGHT_PERCENT: string = '100%';

  /**
   * Full width percent.
   */
  static readonly FULL_WIDTH_PERCENT: string = '100%';

  /**
   * Row width percentage setting.
   */
  static readonly ROW_WIDTH_PERCENT: string = '100%';

  /**
   * Scroll height percentage setting.
   */
  static readonly SCROLL_HEIGHT_PERCENT: string = '100%';

  /**
   * Tech Support column width percentage setting.
   */
  static readonly TECH_COLUMN_WIDTH_PERCENT: string = '100%';

  /**
   * Detail column list width percentage setting.
   */
  static readonly DETAIL_LIST_WIDTH_PERCENT: string = '93.3%';

  /**
   * Divider stroke width.
   */
  static readonly DIVIDER_STROKE_WIDTH: string = '0.5vp';
}