"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const _api_1 = require("./_api");
async function cli() {
    const fs = await Promise.resolve().then(() => require('fs'));
    const path = await Promise.resolve().then(() => require('path'));
    const mime = await Promise.resolve().then(() => require('mime'));
    const os = await Promise.resolve().then(() => require('os'));
    const yargs = await Promise.resolve().then(() => require('yargs/yargs'));
    const hideBin = (await Promise.resolve().then(() => require('yargs/helpers'))).hideBin;
    function parseConfig(config) {
        const defaultConfig = {
            server: 'http://localhost/',
            auth: undefined,
            username: undefined
        };
        try {
            const cfg = JSON.parse(fs.readFileSync(config).toString());
            return Object.assign(defaultConfig, cfg);
        }
        catch (e) {
            return defaultConfig;
        }
    }
    function updateConfig(config, newData) {
        let cfg = {};
        try {
            cfg = JSON.parse(fs.readFileSync(config).toString());
        }
        catch {
            cfg = {};
        }
        Object.assign(cfg, newData);
        fs.writeFileSync(config, JSON.stringify(cfg));
    }
    yargs(hideBin(process.argv))
        .scriptName('northern-files')
        .usage('$0 <cmd> [args]')
        .command('set-server <host>', 'Change the remote server to connect to', (yargs) => {
        yargs.positional('host', {
            type: 'string'
        });
    }, function (argv) {
        updateConfig(argv.config, {
            server: `http://${argv.host}/`
        });
    })
        .command('register <name> <email> <password>', 'Create an account', (yargs) => {
        yargs
            .positional('name', {
            type: 'string'
        })
            .positional('email', {
            type: 'string'
        })
            .positional('password', {
            type: 'string'
        });
    }, async function (argv) {
        const cfg = parseConfig(argv.config);
        const api = new _api_1.Api(cfg.server);
        try {
            const data = await api.register(argv.name, argv.email, argv.password);
            console.log('Created new user with id', data.id);
        }
        catch (e) {
            console.error('ERROR:', e.message);
            process.exit(1);
        }
    })
        .command('login <email> <password>', 'Login to an existing account', (yargs) => {
        yargs
            .positional('email', {
            type: 'string'
        })
            .positional('password', {
            type: 'string'
        });
    }, async function (argv) {
        const cfg = parseConfig(argv.config);
        const api = new _api_1.Api(cfg.server);
        try {
            const data = await api.login(argv.email, argv.password);
            updateConfig(argv.config, Object.assign(cfg, { auth: data.token, username: argv.email }));
            console.log('Logged in successfully');
        }
        catch (e) {
            console.error('ERROR:', e.message);
            process.exit(1);
        }
    })
        .command('list-files', 'Get a list of all your files', () => {
        // Do nothing
    }, async function (argv) {
        const cfg = parseConfig(argv.config);
        const api = new _api_1.Api(cfg.server);
        if (!cfg.username) {
            console.error('ERROR: Unauthenticated');
            process.exit(1);
        }
        try {
            const files = await (async () => {
                if (!cfg.username) {
                    console.error('ERROR: Unauthenticated');
                    process.exit(1);
                }
                if (!argv.legacy) {
                    return await api.getFiles(cfg.username, cfg.auth);
                }
                const readline = (await Promise.resolve().then(() => require('readline'))).createInterface({
                    input: process.stdin,
                    output: process.stdout
                });
                const password = (await new Promise((resolve) => {
                    readline.question('Please insert your password: ', (name) => {
                        readline.close();
                        resolve(name);
                    });
                }));
                return await api.getFiles(cfg.username, undefined, await api.getPrivateKey(cfg.username, password));
            })();
            if (files.length === 0) {
                console.log("You don't have any file!");
            }
            else {
                console.log('Here are your files:');
                files.forEach((file) => {
                    console.log(`    • ${file.id} (${file.metadata})`);
                });
            }
        }
        catch (e) {
            console.error('ERROR:', e.message);
            process.exit(1);
        }
    })
        .command('get-file <file-id> [out-file]', 'Retrieve a file', (yargs) => {
        yargs
            .positional('file-id', {
            type: 'string'
        })
            .positional('out-file', {
            type: 'string'
        });
    }, async function (argv) {
        const cfg = parseConfig(argv.config);
        const api = new _api_1.Api(cfg.server);
        if (!cfg.username) {
            console.error('ERROR: Unauthenticated');
            process.exit(1);
        }
        const readline = (await Promise.resolve().then(() => require('readline'))).createInterface({
            input: process.stdin,
            output: process.stdout
        });
        const password = (await new Promise((resolve) => {
            readline.question('Please insert your password: ', (name) => {
                readline.close();
                resolve(name);
            });
        }));
        try {
            const file = await (async () => {
                if (!cfg.username) {
                    console.error('ERROR: Unauthenticated');
                    process.exit(1);
                }
                if (!argv.legacy) {
                    return await api.getFile(argv['file-id'], cfg.username, password, cfg.auth);
                }
                return await api.getFile(argv['file-id'], cfg.username, password, undefined, await api.getPrivateKey(cfg.username, password));
            })();
            if (argv['out-file']) {
                fs.writeFileSync(argv['out-file'], file);
            }
            else {
                console.log(file.toString());
            }
        }
        catch (e) {
            console.error('ERROR:', e.message);
            process.exit(1);
        }
    })
        .command('upload <file> <metadata>', 'Upload a new file', (yargs) => {
        yargs
            .positional('file', {
            type: 'string'
        })
            .positional('metadata', {
            type: 'string'
        });
    }, async function (argv) {
        const cfg = parseConfig(argv.config);
        const api = new _api_1.Api(cfg.server);
        if (!cfg.username) {
            console.error('ERROR: Unauthenticated');
            process.exit(1);
        }
        try {
            if (!fs.existsSync(argv.file)) {
                throw Error('File is invalid');
            }
            const mime_type = mime.getType(argv.file);
            const fileContent = fs.readFileSync(argv.file);
            const data = await api.upload(cfg.username, fileContent, path.basename(argv.file), mime_type ?? 'text/plain', argv.metadata, cfg.auth);
            console.log('File successfully uploaded!', `${cfg.server}bucket/${data.details}`);
        }
        catch (e) {
            console.error('ERROR:', e.message);
            process.exit(1);
        }
    })
        .command('share <file-id> <user-id>', 'Share a file with a user', (yargs) => {
        yargs
            .positional('file-id', {
            type: 'string'
        })
            .positional('user-id', {
            type: 'string'
        });
    }, async function (argv) {
        const cfg = parseConfig(argv.config);
        const api = new _api_1.Api(cfg.server);
        if (!cfg.username) {
            console.error('ERROR: Unauthenticated');
            process.exit(1);
        }
        const readline = (await Promise.resolve().then(() => require('readline'))).createInterface({
            input: process.stdin,
            output: process.stdout
        });
        const password = (await new Promise((resolve) => {
            readline.question('Please insert your password: ', (name) => {
                readline.close();
                resolve(name);
            });
        }));
        try {
            await api.shareFile(argv['file-id'], cfg.username, argv['user-id'], password, cfg.auth);
            console.log('File shared successfully!');
        }
        catch (e) {
            console.error('ERROR:', e.message);
            process.exit(1);
        }
    })
        .option('config', {
        alias: 'c',
        type: 'string',
        description: 'Path of config file',
        default: os.homedir() + '/.config/.northern-files'
    })
        .option('legacy', {
        description: 'Use legacy autentication mode'
    })
        .deprecateOption('legacy')
        .demandCommand(1)
        .help()
        .strict()
        .parse();
}
if (typeof window === 'undefined') {
    cli();
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
}
else {
    throw Error('This file is not supposed to be used here');
}
