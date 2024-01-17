require("dotenv").config();
const express= require('express')
const app= express()
const bodyParser= require("body-parser")
const mongoose= require('mongoose')
const prompt = require("prompt-sync")();

app.use(bodyParser.json())
mongoose
.connect(process.env.DATABASE)
.then(() => {
    console.log("Database Connected");
});

const deviceSchema= mongoose.Schema({
    name: String,
    state: Boolean
  })
  
const Device= mongoose.model("Device", deviceSchema)

app.get("/", function(req, res) {
    // console.log("Home request initiated");
    res.send("Maamla sahi hai!")
})

const getAllEntities = async () => {
    try {
      const allEntities = await Device.find({});
      return allEntities;
    } catch (error) {
      // console.error('Error fetching entities:', error);
      throw error;
    }
  };

app.get("/get-all-devices", function (req, res) {
    getAllEntities().then(entities => {
        // console.log('All Entities:', entities);
        res.send(entities)
      })
    .catch(function (err) {
        res.redirect("/");
    });
})

const getDeviceStateString = async () => {
    try {
      const devices = await Device.find({}, 'state');
      const stateString = devices.map(device => (device.state ? '1' : '0')).join('');
      // console.log('Device State String:', stateString);
      return stateString;
    } catch (error) {
      // console.error('Error fetching devices:', error);
      throw error;
    }
  };

app.get("/get-all-devices-status", async function (req, res) {
    // console.log("Request to get devices status from: "+req.hostname);   
    try {
        let stt = await getDeviceStateString();
        res.send(String(stt));
    } catch (error) {
        // console.error('Error in processing request:', error);
        res.status(500).send('Internal Server Error');
    }
})

app.post("/valueSent", function(req, res) {
    // console.log("New value recieved from: "+req.hostname);
    var val= req.body.value;
    var deviceName= "test"
    // console.log(req.body);
    if(val == true) {
        // console.log("Turn that light ON baby");
        // res.send("Light ON")
    } else {
        // console.log("Turn that thing off");
        // res.send("Light turned off")
    }
    Device.findOneAndUpdate(
        { name: deviceName },
        { name: deviceName, state: val },
        { new: true }
      )
    .then(updatedEntry => {
        if (updatedEntry) {
        // console.log('Entry updated successfully:', updatedEntry);
        } else {
        // console.log('Entry not found.');
        }
        res.redirect("/");
    })
    .catch(error => {
        // console.error('Error updating entry:', error);
    });
    
})

app.post("/add-new-device", function(req, res) {
    let data= req.body;
    let deviceName= data.name;
    // console.log("Request to add a new device: "+deviceName);

    const naya= new Device({
        name: deviceName,
        state: 0
    });
    naya.save()
    .then(function (models) {
        if(models) {
            // console.log("Added Successfully!");
            // console.log(models);
            res.redirect("/");
        }
    })
    .catch( function (err) {
        // console.log(err);
    });
})

app.post("/intrusion-alert", function(req, res) {
  var data= req.body;
  var timeOfIntrusion= data.source;
  const options = { timeZone: 'Asia/Kolkata', hour12: true, hour: 'numeric', minute: 'numeric', second: 'numeric' };
  const currentTime = new Date().toLocaleTimeString('en-IN', options);
  console.log("Someone intruded in the territory of: "+timeOfIntrusion+ "at: "+currentTime);
  res.send("ok");
})

app.put("/trigger-device", function(req, res) {
    var data= req.body;
    var deviceName= data.name;
    var stateRequest= data.state;

    console.log("Request to trigger device: "+deviceName+", to state: "+stateRequest);
    Device.findOneAndUpdate(
        { name: deviceName },
        { name: deviceName, state: stateRequest },
        { new: true }
      )
    .then(updatedEntry => {
        if (updatedEntry) {
        // console.log('Entry updated successfully:', updatedEntry);
        res.send("ok");
        } else {
        // console.log('Entry not found.');
        }
    })
    .catch(error => {
        // console.error('Error updating entry:', error);
    });
})

app.put("/triggerThroughString", async function(req, res) {

  var data= req.body.newStates;
  var ind= 0;
  try {
    
    const devices = await Device.find();
    const objects = devices;
    if(data.length == objects.length)
    {
      for (let i = 0; i < data.length; i++) {
          const state = data[i] == '1';
          Device.findOneAndUpdate(
            { name: objects[i].name },
            { name: objects[i].name, state: state },
            { new: true }
          )
          .then(updatedEntry => {
              if (updatedEntry) {
              // console.log('Entry updated successfully:', updatedEntry);
              res.send("ok");
              } else {
              // console.log('Entry not found.');
              }
          })
          .catch(error => {
              // console.error('Error updating entry:', error);
          });
      }
    }
    
  } catch (error) {
    console.error('Error fetching and printing names:', error);
  }
  res.send("ok");
})

const shutdownAllEntities = async () => {
    try {
      const devicesToUpdate = await Device.find({ state: { $ne: false } }); // Find devices with state not equal to false
  
      const updatePromises = devicesToUpdate.map(device => {
        device.state = false;
        return device.save();
      });
  
      await Promise.all(updatePromises);
  
      // console.log('Shutdown completed successfully.');
    } catch (error) {
      // console.error('Error during shutdown:', error);
      throw error;
    }
  };
  
  // Route to trigger the shutdown
app.get('/entire-shutdown', async (req, res) => {
try {
    await shutdownAllEntities();
    res.send('Shutdown successful.');
} catch (error) {
    res.status(500).send('Internal Server Error');
}
});
  
const PORT= process.env.PORT||8080;
app.listen(PORT, function () { 
    console.log(`Server listening at port ${PORT}`);
 });
