import { createSha256 } from './sha256.js';
import {
  buildSenderWebSocketUrl,
  hashFileIncremental,
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
let activeSocket = null;

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

function closeActiveSocket(reason) {
  if (activeSocket && activeSocket.readyState < 2) {
    activeSocket.close(1000, reason);
  }
  activeSocket = null;
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

window.addEventListener('pagehide', () => {
  closeActiveSocket('page closed');
});

sendButton.addEventListener('click', async () => {
  const file = fileInput.files?.[0];
  if (!file || !sessionId || busy) return;

  busy = true;
  fileInput.disabled = true;
  updateButton();
  let socket;

  try {
    status.textContent = 'Checking file integrity…';
    const digest = await hashFileIncremental(file, createSha256, (current, total) => {
      setProgress('Checking', current, total);
    });

    status.textContent = 'Connecting to receiver…';
    socket = new WebSocket(buildSenderWebSocketUrl(window.location, sessionId));
    socket.binaryType = 'arraybuffer';
    activeSocket = socket;
    await waitForSocketOpen(socket);

    await sendFile({
      file,
      socket,
      hasherFactory: createSha256,
      sha256Digest: digest,
      onUploadProgress(current, total) {
        status.textContent = 'Sending file…';
        setProgress('Sending', current, total);
      },
    });

    progress.value = 100;
    progressLabel.textContent = 'Sent 100%';
    status.textContent = 'File sent. You can close this page.';
    closeActiveSocket('complete');
  } catch (error) {
    progressLabel.textContent = 'Transfer failed';
    status.textContent = error instanceof Error ? error.message : 'Transfer failed.';
    closeActiveSocket('transfer failed');
    busy = false;
    fileInput.disabled = false;
    updateButton();
  }
});
