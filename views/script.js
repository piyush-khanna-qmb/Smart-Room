// Fetch initial device states from the server
fetch('/get-all-devices-status')
  .then(response => response.text())
  .then(stateString => {
    const stateArray = stateString.split('').map(state => state === '1');
    updateButtonStates(stateArray);
  })
  .catch(error => {
    console.error('Error fetching device states:', error);
  });

function updateButtonStates(states) {
  const buttons = document.querySelectorAll('.power-switch input[type="checkbox"]');
  buttons.forEach((button, index) => {
    button.checked = states[index];
  });
}

function triggerDevice(deviceName, toState) {
  const url = '/trigger-device';
  const data = {
    name: deviceName,
    state: toState
  };

  fetch(url, {
    method: 'PUT',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(data)
  })
  .then(response => {
    if (response.ok) {
      console.log(`Request sent on ${new Date().toISOString()}`);
    } else {
      console.error(`Error: ${response.status}`);
    }
  })
  .catch(error => {
    console.error('Error triggering device:', error);
  });
}

function speak(text) {
  const utterance = new SpeechSynthesisUtterance(text);
  speechSynthesis.speak(utterance);
}

const buttons = document.querySelectorAll('.power-switch input[type="checkbox"]');
buttons.forEach(button => {
  button.addEventListener('change', () => {
    const deviceName = button.parentElement.classList[1];
    const toState = button.checked ? true : false;
    const stateSpeak = button.checked ? "on" : "off";

    triggerDevice(deviceName, toState);
    if (document.getElementsByClassName('speaker')[0].querySelector('input[type="checkbox"]').checked) {
      speak(`The ${deviceName} has been turned ${stateSpeak}.`);
    }
  });
});