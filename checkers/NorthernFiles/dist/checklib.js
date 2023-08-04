"use strict";
// This file is manually generated from checklib.py
// I hope you never have to change this, I'm sorry.
// I never wanted to write a checker in Node, stuff happens
Object.defineProperty(exports, "__esModule", { value: true });
exports.post_flag_id = exports.quit = exports.get_data = exports.Action = exports.Status = void 0;
exports.Status = {
    OK: 101,
    DOWN: 104,
    ERROR: 110
};
exports.Action = {
    CHECK_SLA: 'CHECK_SLA',
    PUT_FLAG: 'PUT_FLAG',
    GET_FLAG: 'GET_FLAG'
};
function get_data() {
    if (!process.env.ACTION || !process.env.TEAM_ID || !process.env.ROUND) {
        quit(exports.Status.ERROR, 'Checker down', 'Did not set necessary environment variables');
    }
    return {
        action: process.env.ACTION,
        teamId: process.env.TEAM_ID,
        vulnboxId: process.env.VULNBOX_ID,
        round: process.env.ROUND,
        flag: process.env.ACTION == exports.Action.PUT_FLAG || process.env.ACTION == exports.Action.GET_FLAG
            ? process.env.FLAG
            : undefined
    };
}
exports.get_data = get_data;
function quit(exit_code, comment = '', debug = '') {
    console.log(comment);
    console.error(debug);
    process.exit(exit_code);
}
exports.quit = quit;
async function post_flag_id(service_id, team_id, flag_id) {
    const res = await fetch(`${process.env.FLAGID_SERVICE}/postFlagId`, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            token: process.env.FLAGID_TOKEN,
            serviceId: service_id,
            teamId: team_id,
            round: parseInt(process.env.ROUND ?? ''),
            flagId: flag_id
        })
    });
    if (!res.ok) {
        throw new Error('Cannot post flag id');
    }
}
exports.post_flag_id = post_flag_id;
