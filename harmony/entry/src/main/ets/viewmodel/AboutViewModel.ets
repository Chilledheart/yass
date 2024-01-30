import { DataItem } from './DataItem';
import { CommonConstants } from '../constants/CommonConstants';
import yass from 'libyass.so';

/**
 * About ability view model.
 */
class YassViewModel {
  /**
   * Get about us information.
   *
   * @return {Array<DataItem>} yassInfo
   */
  getYassInfo(): Array<DataItem> {
    let yassInfo: Array<DataItem> = [];
    let serverHost: DataItem = new DataItem();
    serverHost.title = $r('app.string.server_host');
    serverHost.value = yass.getServerHost();
    yassInfo.push(serverHost);
    let serverSNI: DataItem = new DataItem();
    serverSNI.title = $r('app.string.server_sni');
    serverSNI.value = yass.getServerSNI();
    yassInfo.push(serverSNI);
    let serverPort: DataItem = new DataItem();
    serverPort.title = $r('app.string.server_port');
    serverPort.value = yass.getServerPort().toString();
    yassInfo.push(serverPort);
    let username: DataItem = new DataItem();
    username.title = $r('app.string.username');
    username.value = yass.getUsername();
    yassInfo.push(username);
    let password: DataItem = new DataItem();
    password.title = $r('app.string.password');
    password.value = yass.getPassword();
    yassInfo.push(password);
    let cipher: DataItem = new DataItem();
    cipher.title = $r('app.string.cipher');
    cipher.value = yass.getCipher();
    yassInfo.push(cipher);
    let timeout: DataItem = new DataItem();
    timeout.title = $r('app.string.timeout');
    timeout.value = yass.getTimeout().toString();
    yassInfo.push(timeout);
    return yassInfo;
  }
}

let yassViewModel = new YassViewModel();

export default yassViewModel as YassViewModel;