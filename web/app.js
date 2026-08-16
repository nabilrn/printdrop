const fileInput = document.querySelector('#file');
const selection = document.querySelector('#selection');
const sendButton = document.querySelector('#send');
const status = document.querySelector('#status');

fileInput.addEventListener('change', () => {
  const file = fileInput.files[0];

  if (!file) {
    selection.textContent = 'No file selected';
    sendButton.disabled = true;
    return;
  }

  selection.textContent = `${file.name} - ${file.size.toLocaleString()} bytes`;
  sendButton.disabled = false;
});

sendButton.addEventListener('click', () => {
  status.textContent = 'Transfer transport is not implemented yet.';
});
