function id(id) { return document.getElementById(id); }

function request_handler_empty(code, d) {}

function request_get_json(url, handler = request_handler_empty) {
	fetch(url)
		.then(function(r) {
			if (r.status == 200)
				r.json()
					.then(d => handler(200, d))
					.catch(e => handler(0));
			else
				handler(r.status);
		})
		.catch(e => handler(0));
}

function request_post(url, handler = request_handler_empty) {
	fetch(url, {
			method: "POST"
		}).then(r => handler(r.status))
		.catch(e => handler(0));
}

function request_post_body(url, d, handler = request_handler_empty) {
	fetch(url, {
			method: "POST",
			headers: { "Content-Type": "application/json" },
			body: JSON.stringify(d)
		}).then(r => handler(r.status))
		.catch(e => handler(0));
}
