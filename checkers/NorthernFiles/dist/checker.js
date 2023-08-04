#!/usr/bin/env ts-node
"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const _api_1 = require("./_api");
const checklib = require("./checklib");
const crypto = require("crypto");
const seedrandom = require("seedrandom");
const service_id = 'NorthernFiles';
let team_ip = '';
let team_id = '';
let flag = '';
let action = '';
if (process.argv.indexOf('dev') != -1) {
    action = process.env.ACTION;
    team_ip = '127.0.0.1';
    flag = 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=';
}
else {
    const data = checklib.get_data();
    action = data.action;
    flag = data.flag;
    team_id = data.teamId;
    team_ip = '10.' + (60 + parseInt(data.vulnboxId)) + '.' + data.teamId + '.1';
}
const checkerFetch = async (input, init) => {
    const timeoutId = setTimeout(() => {
        checklib.quit(checklib.Status.DOWN, 'Request timeout', 'Timed out fetch request');
    }, 5000);
    let headers = {};
    if (init?.headers) {
        try {
            if (init.headers.forEach) {
                init.headers.forEach((value, key) => {
                    headers[key] = value;
                });
            }
            else {
                headers = init.headers;
            }
        }
        catch {
            console.error('There was something wrong with these headers');
            console.error(init.headers);
        }
    }
    headers['user-agent'] = 'checker';
    init = init || {};
    init.headers = headers;
    const res = await fetch(input, init);
    clearTimeout(timeoutId);
    return res;
};
function assert(expression, errorMessage) {
    if (!expression) {
        throw Error(errorMessage);
    }
}
function randomChoice(arr) {
    return arr[Math.floor(arr.length * Math.random())];
}
function seededRandomString(generator, length) {
    return Buffer.from(Array.from(new Array(length)).map(() => Math.floor(generator() * 256))).toString('hex');
}
async function put_flag() {
    try {
        const api = new _api_1.Api(`http://${team_ip}/`, checkerFetch);
        // Register
        const name = crypto.randomBytes(16).toString('hex');
        const generator = seedrandom(flag);
        const username = seededRandomString(generator, 14) + '@example.com';
        const password = seededRandomString(generator, 24);
        try {
            await api.register(name, username, password);
        }
        catch (e) {
            checklib.quit(checklib.Status.DOWN, 'Cannot register', e);
        }
        // Login
        const auth = await (async () => {
            try {
                return await api.login(username, password);
            }
            catch (e) {
                checklib.quit(checklib.Status.DOWN, 'Cannot login', e);
            }
        })();
        // Upload file
        const filename = crypto.randomBytes(8).toString('hex');
        const file = await (async () => {
            try {
                return await api.upload(username, Buffer.from(flag, 'binary'), filename, 'text/plain', flag, auth?.token);
            }
            catch (e) {
                checklib.quit(checklib.Status.DOWN, 'Cannot upload files', e);
            }
        })();
        try {
            await checklib.post_flag_id(service_id, team_id, {
                user: username,
                file: file.details.split('/')[1]
            });
        }
        catch (e) {
            checklib.quit(checklib.Status.ERROR, 'Checker error', e);
        }
    }
    catch (e) {
        checklib.quit(checklib.Status.DOWN, 'Cannot put flag', e);
    }
}
async function get_flag() {
    try {
        const api = new _api_1.Api(`http://${team_ip}/`, checkerFetch);
        const generator = seedrandom(flag);
        const username = seededRandomString(generator, 14) + '@example.com';
        const password = seededRandomString(generator, 24);
        // Login
        const auth = await (async () => {
            try {
                return await api.login(username, password);
            }
            catch (e) {
                checklib.quit(checklib.Status.DOWN, 'Cannot login', e);
            }
        })();
        // Upload file
        const files = await (async () => {
            try {
                return await api.getFiles(username, auth.token);
            }
            catch (e) {
                checklib.quit(checklib.Status.DOWN, 'Cannot retrieve files', e);
            }
        })();
        assert(files[0].metadata === flag, 'Cannot retrieve flag');
        const file = await (async () => {
            try {
                return await api.getFile(files[0].id, username, password, auth.token);
            }
            catch (e) {
                checklib.quit(checklib.Status.DOWN, 'Cannot retrieve files', e);
            }
        })();
        assert(file.toString() === flag, 'Cannot retrieve flag');
    }
    catch (e) {
        checklib.quit(checklib.Status.DOWN, 'Cannot retrieve flag', e);
    }
}
async function check_sla() {
    try {
        const api = new _api_1.Api(`http://${team_ip}/`, checkerFetch);
        // Register
        const name = crypto.randomBytes(16).toString('hex');
        const username = crypto.randomBytes(12).toString('hex') + '@example.com';
        const password = crypto.randomBytes(32).toString('base64');
        try {
            await api.register(name, username, password);
        }
        catch (e) {
            checklib.quit(checklib.Status.DOWN, 'Cannot register', e);
        }
        // Login
        const auth = await (async () => {
            try {
                return await api.login(username, password);
            }
            catch (e) {
                checklib.quit(checklib.Status.DOWN, 'Cannot login', e);
            }
        })();
        // List files
        const files = await (async () => {
            try {
                if (randomChoice([true, false])) {
                    return await api.getFiles(username, auth.token);
                }
                else {
                    return await api.getFiles(username, undefined, await api.getPrivateKey(username, password));
                }
            }
            catch (e) {
                checklib.quit(checklib.Status.DOWN, 'Cannot get files', e);
            }
        })();
        assert(files.length === 0, 'Should not have any file');
        let res = await checkerFetch(`http://${team_ip}/logout?url=/`);
        assert(res.ok, 'Request failed');
        assert(res.url.endsWith('/'), 'Should be redirected');
        assert(res.redirected, 'Should be redirected');
        res = await checkerFetch(`http://${team_ip}/logout?url=/bucket/`);
        assert(res.url.endsWith('/bucket/'), 'Should be redirected');
        assert(res.redirected, 'Should be redirected');
        let page = await res.text();
        assert(page.indexOf('<ListBucketResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">') !== -1, 'Bucket is not exposed');
        // Check frontend
        if (randomChoice([true, false])) {
            let res = await checkerFetch(`http://${team_ip}/`);
            assert(res.ok, 'Request failed');
            let page = await res.text();
            assert(page.indexOf('All right reserved 2023') !== -1, 'Home page broken');
            res = await checkerFetch(`http://${team_ip}/dashboard`);
            assert(res.ok, 'Request failed');
            assert(res.url.endsWith('/login'), 'Should be redirected to login');
            assert(res.redirected, 'Should be redirected to login');
            page = await res.text();
            assert(page.indexOf('Register Now') !== -1, 'Login page broken');
            res = await checkerFetch(`http://${team_ip}/dashboard`, {
                headers: {
                    Cookie: `session=${auth.token}`
                }
            });
            assert(res.ok, 'Request failed');
            assert(res.url.endsWith('/dashboard'), 'Should not be redirected to login');
            page = await res.text();
            assert(page.indexOf('t have any files') !== -1, 'Should say no file');
        }
        // Check share functionality?
        if (randomChoice([true, false])) {
            // Register second account
            const name2 = crypto.randomBytes(16).toString('hex');
            const username2 = crypto.randomBytes(12).toString('hex') + '@example.com';
            const password2 = crypto.randomBytes(32).toString('base64');
            try {
                await api.register(name2, username2, password2);
            }
            catch (e) {
                checklib.quit(checklib.Status.DOWN, 'Cannot register', e);
            }
            // Login second account
            const auth2 = await (async () => {
                try {
                    return await api.login(username2, password2);
                }
                catch (e) {
                    checklib.quit(checklib.Status.DOWN, 'Cannot login', e);
                }
            })();
            // Upload file
            const fileContent = crypto.randomBytes(Math.floor(Math.random() * 1024 + 1));
            const filename = crypto.randomBytes(8).toString('hex');
            const file = await (async () => {
                try {
                    return await api.upload(username, fileContent, filename, 'text/plain', crypto.randomBytes(64).toString('hex'), auth?.token);
                }
                catch (e) {
                    checklib.quit(checklib.Status.DOWN, 'Cannot upload files', e);
                }
            })();
            // List files
            const files1 = await (async () => {
                try {
                    if (randomChoice([true, false])) {
                        return await api.getFiles(username, auth.token);
                    }
                    else {
                        return await api.getFiles(username, undefined, await api.getPrivateKey(username, password));
                    }
                }
                catch (e) {
                    checklib.quit(checklib.Status.DOWN, 'Cannot get files', e);
                }
            })();
            assert(files1.length === 1, 'Cannot upload files');
            assert(files1[0].id === file.details.split('/')[1], 'Cannot upload files');
            // Share file
            await (async () => {
                try {
                    return await api.shareFile(file.details.split('/')[1], username, username2, password, auth.token);
                }
                catch (e) {
                    checklib.quit(checklib.Status.DOWN, 'Cannot share files', e);
                }
            })();
            // Check file is there
            const files2 = await (async () => {
                try {
                    if (randomChoice([true, false])) {
                        return await api.getFiles(username2, auth2.token);
                    }
                    else {
                        return await api.getFiles(username2, undefined, await api.getPrivateKey(username2, password2));
                    }
                }
                catch (e) {
                    checklib.quit(checklib.Status.DOWN, 'Cannot get files', e);
                }
            })();
            assert(files2.length === 1, 'Cannot upload files');
            assert(files2[0].id === file.details.split('/')[1], 'Cannot upload files');
            // Read file
            const file2 = await (async () => {
                try {
                    if (randomChoice([true, false])) {
                        return await api.getFile(files2[0].id, username2, password2, auth2.token);
                    }
                    else {
                        return await api.getFile(files2[0].id, username2, password2, undefined, await api.getPrivateKey(username2, password2));
                    }
                }
                catch (e) {
                    checklib.quit(checklib.Status.DOWN, 'Cannot read shared file', e);
                }
            })();
            assert(file2.toString('binary') === fileContent.toString('binary'), 'Shared file cannot be opened');
            if (randomChoice([true, false])) {
                // Verify you cannot reshare
                await (async () => {
                    try {
                        await api.shareFile(file.details.split('/')[1], username2, username, password2, auth2.token);
                        checklib.quit(checklib.Status.DOWN, 'Broken file sharing', 'User should not be able to share');
                    }
                    catch (e) { }
                })();
            }
            // Check frontend
            if (randomChoice([true, false])) {
                let res = await checkerFetch(`http://${team_ip}/dashboard`, {
                    headers: {
                        Cookie: `session=${auth.token}`
                    }
                });
                assert(res.ok, 'Request failed');
                assert(res.url.endsWith('/dashboard'), 'Should not be redirected to login');
                let page = await res.text();
                assert(page.indexOf("You don't have any files") === -1, 'Should have file');
                assert(page.indexOf(filename) !== -1, 'Should have file');
            }
        }
    }
    catch (e) {
        checklib.quit(checklib.Status.DOWN, 'Service broken', e);
    }
}
(async () => {
    try {
        if (action == checklib.Action.CHECK_SLA)
            await check_sla();
        else if (action == checklib.Action.PUT_FLAG)
            await put_flag();
        else if (action == checklib.Action.GET_FLAG)
            await get_flag();
    }
    catch (e) {
        checklib.quit(checklib.Status.DOWN, 'Service raised an exception', e);
    }
    checklib.quit(checklib.Status.OK, 'OK');
})();
// Weird code to silence the experimental fetch message
const { emit: originalEmit } = process;
// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-ignore
process.emit = (event, error) => 
// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-ignore
event === 'warning' && error.name === 'ExperimentalWarning'
    ? false
    : // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-ignore
        originalEmit.apply(process, arguments);
