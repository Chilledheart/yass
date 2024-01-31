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
  timeout: number = 0;
}