


async function createShareLink() {
  const filename = document.getElementById('filename').textContent;
  const apiPath = '/api/share/';

  let resp = await fetch(apiPath + filename);
  resp = await resp.json()
  alert('Here is your link: ' + resp['link']);
}


async function report(filename) {
  const apiPath = '/api/ban';

  let resp = await fetch(apiPath);

}

async function runMacro() {
  const apiPath = '/api/run';
  const filename = document.getElementById('filename').textContent;
  
  data = {
    filename: filename
  }

  const req = await fetch(url, {
    method: "POST",
    mode: "cors",
    cache: "no-cache",
    credentials: "same-origin",
    headers: {
      "Content-Type": "application/json",

    },
    referrerPolicy: "no-referrer",
    body: JSON.stringify(data),
  });

  const resp = await req.text();

  document.getElementById("scrollContent").innerHTML = marked.parse(
    `**${filename}**<br/>${resp}`
  );

}



