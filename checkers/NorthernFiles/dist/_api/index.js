"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Api = void 0;
const index_js_1 = require("openapi-fetch/dist/index.cjs");
const getClients = (baseUrl, _fetch) => {
    const apiClient = (0, index_js_1.default)({
        baseUrl: `${baseUrl}api`,
        fetch: _fetch
    });
    const authClient = (0, index_js_1.default)({
        baseUrl: `${baseUrl}api/auth`,
        fetch: _fetch
    });
    return {
        apiClient,
        authClient
    };
};
class Api {
    constructor(basePath = '/', _fetch = fetch) {
        this.getPrivateKey = async (username, password) => {
            try {
                const userInfo = await this.getUserInfo(username);
                const CryptoLib = (await this.CryptoLibPromise)();
                const capsule = JSON.parse(atob(userInfo.capsule));
                return CryptoLib.privkey_from_capsule(capsule, password);
            }
            catch {
                throw Error('Wrong password');
            }
        };
        this.getCurrentUser = async (authToken) => {
            const { error, data, response } = await this.apiClient.get('/user', {
                credentials: 'include',
                headers: authToken
                    ? {
                        Cookie: `session=${authToken}`
                    }
                    : undefined
            });
            if (!data || error || !response.ok) {
                return null;
            }
            return data;
        };
        this.getUserInfo = async (email) => {
            const { error, data, response } = await this.authClient.get('/user/{username}', {
                params: {
                    path: {
                        username: email
                    }
                }
            });
            if (!data || error || !response.ok) {
                throw Error('User not found');
            }
            return data;
        };
        this.login = async (email, password) => {
            try {
                const CryptoLib = (await this.CryptoLibPromise)();
                const [client_step_1, client_storage] = CryptoLib.client_step1(email, password);
                const { error, data, response } = await this.authClient.post('/user/{username}/challenge', {
                    params: {
                        path: {
                            username: email
                        }
                    },
                    body: client_step_1
                });
                if (!data || error || !response.ok) {
                    throw Error('Server error');
                }
                const ss_id = data.ss_id;
                const client_step_2 = CryptoLib.client_step2(data, client_storage);
                {
                    const { error, data, response } = await this.authClient.post('/user/{username}/session', {
                        params: {
                            path: {
                                username: email
                            }
                        },
                        body: {
                            ss_id: ss_id,
                            ...client_step_2
                        }
                    });
                    if (!data || error || !response.ok) {
                        throw Error('Wrong email or password');
                    }
                    return data;
                }
            }
            catch {
                throw Error('Wrong email or password');
            }
        };
        this.register = async (name, email, password) => {
            const CryptoLib = (await this.CryptoLibPromise)();
            const capsule = CryptoLib.register_user(email, password);
            if (capsule.success !== true) {
                throw Error('Cannot create cryptographic keys');
            }
            const { error, data, response } = await this.authClient.post('/user', {
                body: {
                    name: name,
                    username: email,
                    capsule: btoa(JSON.stringify(capsule))
                }
            });
            if (!response.ok) {
                if (response.status === 409) {
                    throw Error('Email address is already in use');
                }
                else {
                    throw Error('Error while registering a new user');
                }
            }
            if (error) {
                throw Error('Error while registering a new user');
            }
            return data;
        };
        this.getFiles = async (username, authToken, privateKey) => {
            let headers = authToken
                ? {
                    Cookie: `session=${authToken}`
                }
                : {};
            if (privateKey) {
                const crypto = await Promise.resolve().then(() => require('crypto'));
                const key = crypto.createPrivateKey({
                    key: Buffer.from(privateKey, 'hex'),
                    format: 'der',
                    type: 'pkcs1'
                });
                const sign = crypto.createSign('SHA512');
                sign.update('GET/api/files');
                sign.end();
                const signature = sign.sign(key);
                headers = {
                    'X-Request-Signature': signature.toString('hex'),
                    'X-Request-User': username
                };
            }
            const { error, data, response } = await this.apiClient.get('/files', {
                credentials: 'include',
                headers: headers
            });
            if (!data || error || !response.ok) {
                if (response.status === 401) {
                    throw Error('Unauthorized');
                }
                else {
                    throw Error('Server error');
                }
            }
            return data;
        };
        this.getFile = async (id, username, password, authToken, privateKey) => {
            let headers = authToken
                ? {
                    Cookie: `session=${authToken}`
                }
                : {};
            if (privateKey) {
                const crypto = await Promise.resolve().then(() => require('crypto'));
                const key = crypto.createPrivateKey({
                    key: Buffer.from(privateKey, 'hex'),
                    format: 'der',
                    type: 'pkcs1'
                });
                const sign = crypto.createSign('SHA512');
                sign.update(`GET/api/files/${id}`);
                sign.end();
                const signature = sign.sign(key);
                headers = {
                    'X-Request-Signature': signature.toString('hex'),
                    'X-Request-User': username
                };
            }
            const userInfo = await this.getUserInfo(username);
            const CryptoLib = (await this.CryptoLibPromise)();
            const capsule = JSON.parse(atob(userInfo.capsule));
            const { error, data: fileData, response } = await this.apiClient.get('/files/{id}', {
                params: {
                    path: {
                        id: id
                    }
                },
                credentials: 'include',
                headers: headers
            });
            if (!fileData || error || !response.ok) {
                if (response.status === 401) {
                    throw Error('Unauthorized');
                }
                else if (response.status === 404) {
                    throw Error('File not found');
                }
                else {
                    throw Error('Server error');
                }
            }
            const fileRes = await this._fetch(`${this.basePath}bucket/${fileData.owner}/${fileData.id}`);
            if (!fileRes.ok) {
                throw Error('Cannot retrieve file');
            }
            const fileRaw = await fileRes.text();
            try {
                const file = CryptoLib.retrieve_file(fileRaw, fileData.key, capsule, password);
                if (typeof window !== 'undefined') {
                    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
                    // @ts-ignore
                    return ethereumjs.Buffer.Buffer.from(file, 'binary');
                }
                else {
                    return Buffer.from(file, 'binary');
                }
            }
            catch {
                throw Error('Cannot decrypt file');
            }
        };
        this.shareFile = async (id, username, receiver, password, authToken) => {
            const userInfo = await this.getUserInfo(username);
            const receiverInfo = await this.getUserInfo(receiver);
            const CryptoLib = (await this.CryptoLibPromise)();
            const senderCapsule = JSON.parse(atob(userInfo.capsule));
            const receiverCapsule = JSON.parse(atob(receiverInfo.capsule));
            const { error, data: fileData, response } = await this.apiClient.get('/files/{id}', {
                params: {
                    path: {
                        id: id
                    }
                },
                credentials: 'include',
                headers: authToken
                    ? {
                        Cookie: `session=${authToken}`
                    }
                    : undefined
            });
            if (error || !fileData || !response.ok) {
                throw Error('File not found');
            }
            let key;
            try {
                key = CryptoLib.share_file(senderCapsule, receiverCapsule, password, fileData.key);
            }
            catch {
                throw Error('Cannot retrieve and share encryption key');
            }
            {
                const { error, data, response } = await this.apiClient.post('/files/{id}/share', {
                    params: {
                        path: {
                            id: id
                        }
                    },
                    body: {
                        key: key,
                        user: receiver
                    },
                    credentials: 'include',
                    headers: authToken
                        ? {
                            Cookie: `session=${authToken}`
                        }
                        : undefined
                });
                if (error || !data || !response.ok) {
                    if (response.status === 401) {
                        throw Error('Unauthorized');
                    }
                    else if (response.status === 404) {
                        throw Error('File not found');
                    }
                    else if (response.status === 409) {
                        throw Error('User already has access to this file');
                    }
                    else {
                        throw Error('Server error');
                    }
                }
            }
        };
        this.upload = async (username, file, name, mime_type, metadata, authToken) => {
            const userInfo = await this.getUserInfo(username);
            const CryptoLib = (await this.CryptoLibPromise)();
            const capsule = JSON.parse(atob(userInfo.capsule));
            const [encrypted_file, key] = (() => {
                try {
                    return CryptoLib.encrypt_file(file.toString('binary'), capsule);
                }
                catch {
                    throw Error('Cannot encrypt file');
                }
            })();
            try {
                const { error, data: uploadData, response } = await this.apiClient.post('/files', {
                    body: {
                        name: name,
                        key: key,
                        metadata: btoa(metadata),
                        mime_type: mime_type
                    },
                    credentials: 'include',
                    headers: authToken
                        ? {
                            Cookie: `session=${authToken}`
                        }
                        : undefined
                });
                if (error || !uploadData || !response.ok) {
                    throw Error('Cannot upload file');
                }
                const res = await this._fetch(uploadData.upload_url, {
                    method: 'PUT',
                    headers: {
                        'Content-Type': mime_type
                    },
                    body: encrypted_file
                });
                if (!res.ok) {
                    throw Error('Cannot upload file');
                }
                {
                    const { error, data, response } = await this.apiClient.put('/files/{id}', {
                        params: {
                            path: {
                                id: uploadData.id
                            }
                        },
                        body: {
                            token: uploadData.token
                        },
                        credentials: 'include',
                        headers: authToken
                            ? {
                                Cookie: `session=${authToken}`
                            }
                            : undefined
                    });
                    if (error || !data || !response.ok) {
                        throw Error('Cannot upload file');
                    }
                    return data;
                }
            }
            catch {
                throw Error('Cannot upload file');
            }
        };
        this._fetch = _fetch;
        this.basePath = basePath;
        const { authClient, apiClient } = getClients(basePath, _fetch);
        this.authClient = authClient;
        this.apiClient = apiClient;
        if (typeof window === 'undefined') {
            this.CryptoLibPromise = Promise.resolve().then(() => require('./cryptolibnode.js'));
        }
        else {
            this.CryptoLibPromise = Promise.resolve(() => {
                // eslint-disable-next-line @typescript-eslint/ban-ts-comment
                // @ts-ignore
                return window.Module;
            });
        }
    }
}
exports.Api = Api;
