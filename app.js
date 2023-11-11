const express= require('express')
const app= express()
const bodyParser= require("body-parser")

app.use(bodyParser.json())

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

app.listen(8080, function () { 
    console.log("Server listening at port 8080");
 });
