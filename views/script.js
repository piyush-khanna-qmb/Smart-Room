// Keep track of pending updates
let pendingUpdates = new Map();
let speakQueue = [];
let isSpeaking = false;

// Function to fetch and update device states
function fetchAndUpdateStates() {
  // Don't fetch if there are pending updates
  if (pendingUpdates.size > 0) {
    return;
  }

  fetch('/get-all-devices-status')
    .then(response => response.text())
    .then(stateString => {
      const stateArray = stateString.split('').map(state => state === '1');
      updateButtonStates(stateArray);
    })
    .catch(error => {
      console.error('Error fetching device states:', error);
    });
}

// Function to update button states without triggering events
function updateButtonStates(states) {
  const buttons = document.querySelectorAll('.power-switch input[type="checkbox"]');
  buttons.forEach((button, index) => {
    const deviceName = button.parentElement.classList[1];
    // Skip the speaker button in the auto-refresh
    if (deviceName === 'speaker') return;
    
    // Only update if there's no pending update for this device
    if (!pendingUpdates.has(deviceName) && button.checked !== states[index]) {
      // Temporarily remove event listener
      const oldButton = button.cloneNode(true);
      button.parentNode.replaceChild(oldButton, button);
      oldButton.checked = states[index];
      
      // Reattach event listener to new button
      attachEventListener(oldButton);
    }
  });
}
function triggerDevice(deviceName, toState) {
  // Add to pending updates
  pendingUpdates.set(deviceName, toState);

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
      // Wait a short delay before removing from pending updates
      setTimeout(() => {
        pendingUpdates.delete(deviceName);
      }, 1000);

      // Check if the speaker is turned on, and then speak the state change
      if (document.getElementsByClassName('speaker')[0].querySelector('input[type="checkbox"]').checked) {
        const stateSpeak = toState ? "on" : "off";
        speak(`The ${deviceName} has been turned ${stateSpeak}.`);
      }
    } else {
      console.error(`Error: ${response.status}`);
      pendingUpdates.delete(deviceName);
    }
  })
  .catch(error => {
    console.error('Error triggering device:', error);
    pendingUpdates.delete(deviceName);
  });
}

function speak(text) {
  // Add text to queue
  speakQueue.push(text);
  processSpeak();
}

function processSpeak() {
  if (isSpeaking || speakQueue.length === 0) return;
  
  isSpeaking = true;
  const utterance = new SpeechSynthesisUtterance(speakQueue[0]);
  
  utterance.onend = () => {
    isSpeaking = false;
    speakQueue.shift(); // Remove the spoken text
    processSpeak(); // Process next in queue if any
  };

  utterance.onerror = () => {
    isSpeaking = false;
    speakQueue.shift(); // Remove the failed text
    processSpeak(); // Try next in queue
  };

  speechSynthesis.speak(utterance);
}

function attachEventListener(button) {
  button.addEventListener('change', () => {
    const deviceName = button.parentElement.classList[1];
    const toState = button.checked ? true : false;
    const stateSpeak = button.checked ? "on" : "off";


    if (deviceName !== 'speaker') {
      triggerDevice(deviceName, toState);
    }
    

    if (deviceName === 'speaker') {
      // If speaker is turned off, clear the queue
      if (!toState) {
        speakQueue = [];
        speechSynthesis.cancel();
        isSpeaking = false;
      }
    } 
  });
}

function initializeEventListeners() {
  const buttons = document.querySelectorAll('.power-switch input[type="checkbox"]');
  buttons.forEach(button => {
    attachEventListener(button);
  });
}


fetchAndUpdateStates();
initializeEventListeners();

const pollingInterval = setInterval(fetchAndUpdateStates, 5000);

function setVideoSource() {
  const videoElement = document.getElementById("backgroundVideo");
  const sourceElement = document.getElementById("videoSource");

  // Check screen width and set video source accordingly
  if (window.innerWidth < 768) {
    sourceElement.src = "ai bg.mp4"; // Video for small screens
  } else {
    sourceElement.src = "ai1.mp4"; // Video for large screens
  }

  // Reload the video to apply the new source
  videoElement.load();
}

// Run on initial load
setVideoSource();

// Listen for window resize to update video source dynamically
window.addEventListener("resize", setVideoSource);

const micSwitch = document.querySelector('.mic-switch input');
const speechOutput = document.getElementById('speechOutput');

// Check if the browser supports speech recognition
if ('webkitSpeechRecognition' in window) {
  const recognition = new webkitSpeechRecognition();

  // Configure speech recognition
  recognition.continuous = false;
  recognition.interimResults = true;
  recognition.lang = 'en-IN';

  let recognitionActive = false;
  let finalTranscript = ''; // Store final results cumulatively

  // Event handlers
  recognition.onstart = () => {
    recognitionActive = true;
    // Turn on the microphone button
    micSwitch.checked = true;
    speechOutput.textContent = 'Listening...';
    finalTranscript = '';
    console.log("Started");
  };

  recognition.onend = () => {
    recognitionActive = false;
    // Turn off the microphone button
    micSwitch.checked = false;
    processCommand(speechOutput.textContent);
    console.log("Ended");
  };

  recognition.onresult = (event) => {
    speechOutput.textContent = 'Listening...';
    let interimTranscript = ''; // Store interim results for the current event
    for (let i = event.resultIndex; i < event.results.length; i++) {
      if (event.results[i].isFinal) {
        finalTranscript += event.results[i][0].transcript; // Add to final transcript
      } else {
        interimTranscript += event.results[i][0].transcript; // Add to interim transcript
      }
    }

    // Display the cumulative final transcript and current interim transcript
    speechOutput.textContent = finalTranscript + interimTranscript;
  };

  // Start and stop speech recognition when the microphone button is clicked
  micSwitch.addEventListener('click', () => {
    if (!recognitionActive) {
      recognition.start();
    } else {
      recognition.stop();
    }
  });
} else {
  // If the browser doesn't support speech recognition, display an error message
  speechOutput.textContent = 'Speech recognition is not supported in this browser.';
}
// List of devices to check
const deviceSynonyms = {
  'fan': ['fan'],
  'tubelight': ['tubelight', 'tube light'], // Handle both "tubelight" and "tube light"
  'tabletop': ['tabletop', 'table top', 'table top light'],
  'nightbulb': ['nightbulb', 'night bulb'],
  'laser': ['laser', 'discolight', 'disco light'],
  'bluelight': ['bluelight', 'blue light'],
  'speaker': ['speaker'],
  'mic': ['mic', 'microphone']
};
function normalizeDeviceName(deviceName) {
  // Loop through each device and check if the command contains any synonym
  for (const [standardName, synonyms] of Object.entries(deviceSynonyms)) {
    if (synonyms.includes(deviceName.toLowerCase())) {
      return standardName; // Return the standardized name
    }
  }
  return null; // If no match is found
}

// Function to process the command and trigger devices
function processCommand(command) {
  // Normalize the command to lower case and trim spaces
  const normalizedCommand = command.trim().toLowerCase();

  // Check if the command contains "turn on" or "turn off"
  const actionMatch = normalizedCommand.match(/turn (on|off)/i);
  if (!actionMatch) {
    console.error("Invalid command format. Please use 'turn on <device_name>' or 'turn off <device_name>'.");
    return;
  }
  
  // Extract action (on/off)
  const action = actionMatch[1].toLowerCase();
  let devicesToControl = [];

  // Split the command into sections based on 'and' and process each part
  const commandSections = normalizedCommand.split(/and/).map(section => section.trim());

  commandSections.forEach(section => {
    // Loop through all synonyms to check for device mentions in the section
    Object.entries(deviceSynonyms).forEach(([standardDeviceName, synonyms]) => {
      synonyms.forEach(synonym => {
        if (section.includes(synonym)) {
          devicesToControl.push(standardDeviceName);
        }
      });
    });
  });

  // If no valid devices are found
  if (devicesToControl.length === 0) {
    console.error("No valid devices found in the command.");
    return;
  }

  // Iterate through the devices to control
  devicesToControl = [...new Set(devicesToControl)]; // Remove duplicates
  devicesToControl.forEach(deviceName => {
    const deviceElement = document.querySelector(`.power-switch.${deviceName} input[type="checkbox"]`);
    if (deviceElement) {
      const targetState = action === 'on';
      if (deviceElement.checked !== targetState) {
        deviceElement.checked = targetState;
        triggerDevice(deviceName, targetState);
        console.log(`Command processed: Turn ${action} ${deviceName}`);
      } else {
        console.log(`${deviceName} is already ${action}`);
      }
    } else {
      console.error(`Device "${deviceName}" not found.`);
    }
  });
}