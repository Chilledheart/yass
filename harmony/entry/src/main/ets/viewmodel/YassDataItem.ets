// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

/**
 * Basic list item data bean.
 */
export class YassDataItem {
  serverHost: string = '';
  serverSNI: string = '';
  serverPort: number = 0;
  username: string = '';
  password: string = '';
  cipher: string = '';
  dohUrl: string = '';
  dotHost: string = '';
  limitRate: string = '';
  timeout: number = 0;
}
