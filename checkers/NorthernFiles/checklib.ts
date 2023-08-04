// This file is manually generated from checklib.py
// I hope you never have to change this, I'm sorry.
// I never wanted to write a checker in Node, stuff happens

export const Status = {
	OK: 101,
	DOWN: 104,
	ERROR: 110
};

export const Action = {
	CHECK_SLA: 'CHECK_SLA',
	PUT_FLAG: 'PUT_FLAG',
	GET_FLAG: 'GET_FLAG'
};

declare global {
	namespace NodeJS {
		interface ProcessEnv {
			ACTION: string;
			TEAM_ID: string;
			VULNBOX_ID: string;
			ROUND: string;
		}
	}
}

export function get_data() {
	if (!process.env.ACTION || !process.env.TEAM_ID || !process.env.ROUND) {
		quit(Status.ERROR, 'Checker down', 'Did not set necessary environment variables');
	}
	return {
		action: process.env.ACTION,
		teamId: process.env.TEAM_ID,
		vulnboxId: process.env.VULNBOX_ID,
		round: process.env.ROUND,
		flag:
			process.env.ACTION == Action.PUT_FLAG || process.env.ACTION == Action.GET_FLAG
				? process.env.FLAG
				: undefined
	};
}

export function quit(exit_code: number, comment: unknown = '', debug: unknown = '') {
	console.log(comment);
	console.error(debug);
	process.exit(exit_code);
}

export async function post_flag_id(service_id: string, team_id: string, flag_id: any) {
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
