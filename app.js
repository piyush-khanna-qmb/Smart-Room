const express= require('express')
const app= express()
const bodyParser= require("body-parser")
const mongoose= require('mongoose')
const prompt = require("prompt-sync")();

app.use(bodyParser.json())
mongoose.connect("mongodb://127.0.0.1:27017/roomDB")

const deviceSchema= mongoose.Schema({
    name: String,
    state: Boolean
  })
  
const Device= mongoose.model("Device", deviceSchema)

app.get("/", function(req, res) {
    console.log("Home request initiated");
    res.send("Maamla sahi hai!")
})

const getDeviceStateString = async () => {
    try {
      const devices = await Device.find({}, 'state');
      const stateString = devices.map(device => (device.state ? '1' : '0')).join('');
      console.log('Device State String:', stateString);
      return stateString;
    } catch (error) {
      console.error('Error fetching devices:', error);
      throw error;
    }
  };

app.get("/get-all-devices-status", async function (req, res) {
    console.log("Request to get devices status from: "+req.hostname);   
    try {
        let stt = await getDeviceStateString();
        res.send(String(stt));
    } catch (error) {
        console.error('Error in processing request:', error);
        res.status(500).send('Internal Server Error');
    }
})

app.post("/valueSent", function(req, res) {
    console.log("New value recieved from: "+req.hostname);
    var val= req.body.value;
    console.log(req.body);
    if(val == 1) {
        console.log("Turn that light ON baby");
        res.send("Light ON")
    } else {
        console.log("Turn that shit off");
        res.send("Light turned off")
    }
})

app.post("/add-new-device", function(req, res) {
    let data= req.body;
    let deviceName= data.name;
    console.log("Request to add a new device: "+deviceName);

    const naya= new Device({
        name: deviceName,
        state: 0
    });
    naya.save()
    .then(function (models) {
        if(models) {
            console.log("Added Successfully!");
            console.log(models);
            res.redirect("/");
        }
    })
    .catch( function (err) {
        console.log(err);
    });
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
        console.log('Entry updated successfully:', updatedEntry);
        res.redirect("/");
        } else {
        console.log('Entry not found.');
        }
    })
    .catch(error => {
        console.error('Error updating entry:', error);
    });
})

app.listen(8080, function () { 
    console.log("Server listening at port 8080");
 });
