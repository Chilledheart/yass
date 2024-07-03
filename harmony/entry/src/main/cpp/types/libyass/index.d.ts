export const getTransferRate: () => string[];
export const getServerHost: () => string;
export const getServerSNI: () => string;
export const getServerPort: () => number;
export const getUsername: () => string;
export const getPassword: () => string;
export const getCipher: () => string;
export const getCipherStrings: () => string[];
export const getDoHUrl: () => string;
export const getDoTHost: () => string;
export const getLimitRate: () => string;
export const getTimeout: () => number;
export const init: (temp_dir: string, data_dir: string) => void;
export const destroy: () => void;
export const startWorker: (cb: (err_msg: string, port: number) => void) => void;
export const stopWorker: (cb: () => void) => void;
export const saveConfig: (server_host: string, server_sni: string, server_port: string,
                          username: string, password: string, cipher: string, doh_url: string,
                          dot_host: string, limit_rate: string, timeout: string) => string;
