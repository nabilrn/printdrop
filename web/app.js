import {
  buildSenderWebSocketUrl,
  parseSessionId,
  sendFile,
  waitForSocketOpen,
} from './sender.js';

const fileInput = document.querySelector('#file');
const selection = document.querySelector('#selection');
const sendButton = document.querySelector('#send');
const status = document.querySelector('#status');
const progress = document.querySelector('#progress');
const progressLabel = document.querySelector('#progress-label');

const sessionId = parseSessionId(window.location.pathname);
let busy = false;

function formatBytes(value) {
  if (value < 1024) return `${value} B`;
  const units = ['KB', 'MB', 'GB', 'TB'];
  let amount = value;
  let unit = -1;
  do {
    amount /= 1024;
    unit += 1;
  } while (amount >= 1024 && unit < units.length - 1);
  return `${amount.toFixed(amount >= 10 ? 1 : 2)} ${units[unit]}`;
}

function updateButton() {
  sendButton.disabled = busy || !sessionId || !fileInput.files?.[0];
}

function setProgress(label, current, total) {
  const ratio = total === 0 ? 1 : Math.min(1, current / total);
  progress.value = Math.round(ratio * 100);
  progressLabel.textContent = `${label} ${Math.round(ratio * 100)}%`;
}

if (!sessionId) {
  status.textContent = 'This PrintDrop link is invalid.';
  fileInput.disabled = true;
} else {
  status.textContent = 'Choose a file to send to this receiver.';
}
updateButton();

fileInput.addEventListener('change', () => {
  const file = fileInput.files?.[0];
  if (!file) {
    selection.textContent = 'No file selected';
    progress.value = 0;
    progressLabel.textContent = '';
  } else {
    selection.textContent = `${file.name} · ${formatBytes(file.size)}`;
    status.textContent = 'Ready to send.';
  }
  updateButton();
});

sendButton.addEventListener('click', async () => {
  const file = fileInput.files?.[0];
  if (!file || !sessionId || busy) return;
  if (!globalThis.sha256?.create) {
    status.textContent = 'SHA-256 component failed to load.';
    return;
  }

  busy = true;
  fileInput.disabled = true;
  updateButton();
  let socket;

  try {
    status.textContent = 'Connecting to receiver…';
    socket = new WebSocket(buildSenderWebSocketUrl(window.location, sessionId));
    socket.binaryType = 'arraybuffer';
    await waitForSocketOpen(socket);

    status.textContent = 'Checking file integrity…';
    await sendFile({
      file,
      socket,
      hasherFactory: () => globalThis.sha256.create(),
      onHashProgress(current, total) {
        setProgress('Checking', current, total);
      },
      onUploadProgress(current, total) {
        status.textContent = 'Sending file…';
        setProgress('Sending', current, total);
      },
    });

    progress.value = 100;
    progressLabel.textContent = 'Sent 100%';
    status.textContent = 'File sent. You can close this page.';
    socket.close(1000, 'complete');
  } catch (error) {
    progressLabel.textContent = 'Transfer failed';
    status.textContent = error instanceof Error ? error.message : 'Transfer failed.';
    if (socket && socket.readyState < 2) {
      socket.close(1011, 'transfer failed');
    }
    busy = false;
    fileInput.disabled = false;
    updateButton();
  }
});
