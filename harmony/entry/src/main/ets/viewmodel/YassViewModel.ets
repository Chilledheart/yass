// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2024 Chilledheart  */

import { YassDataItem } from './YassDataItem';
import yass from 'libyass.so';

/**
 * About ability view model.
 */
class YassViewModel {
  getYassData(): YassDataItem {
    let yassData: YassDataItem = new YassDataItem();
    yassData.serverHost = yass.getServerHost();
    yassData.serverSNI = yass.getServerSNI();
    yassData.serverPort = yass.getServerPort();
    yassData.username = yass.getUsername();
    yassData.password = yass.getPassword();
    yassData.cipher = yass.getCipher();
    yassData.timeout = yass.getTimeout();
    return yassData;
  }
}

let yassViewModel = new YassViewModel();

export default yassViewModel as YassViewModel;