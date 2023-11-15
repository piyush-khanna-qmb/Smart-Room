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

app.get("/add-new-device", function(req, res) {
    console.log("Ready to recieve new device data");
    // let thisName= prompt("Enter new device's name")
    const naya= new Device({
        name: "fan",
        state: 0
    });
    naya.save()
    .then(function (models) {
        if(models) {
            console.log(models);
            console.log("Light add kar di hai");
            res.redirect("/");
        }
    })
    .catch( function (err) {
        console.log(err);
    });
})

app.listen(8080, function () { 
    console.log("Server listening at port 8080");
 });
