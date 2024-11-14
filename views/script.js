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
    } else if (document.getElementsByClassName('speaker')[0].querySelector('input[type="checkbox"]').checked) {
      speak(`The ${deviceName} has been turned ${stateSpeak}.`);
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