export const getTransferRate: () => string[];
export const getServerHost: () => string;
export const getServerSNI: () => string;
export const getServerPort: () => number;
export const getUsername: () => string;
export const getPassword: () => string;
export const getCipher: () => string;
export const getCipherStrings: () => string[];
export const getTimeout: () => number;
export const init: () => void;
export const destroy: () => void;
export const startWorker: (cb: (err_msg: string) => void) => void;
export const stopWorker: (cb: () => void) => void;
export const saveConfig: (server_host: string, server_sni: string, server_port: string,
                          username: string, password: string, cipher: string, timeout: string) => string;
