#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

const char debug_log[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
    <head>
        <title>Message Board</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
    </head>

    <body>
        <h5>MOST RECENT</h5>
        <div id="message-container"></div>

        <script>
             function displayNewMessage(newMessage){
                var card = document.createElement("div");
                card.classList.add("message-card"); 
                card.innerHTML = newMessage.body;

                document.getElementById('message-container').prepend(card);
            }
            function debugLogListener () {
                var data = JSON.parse(this.responseText);
                data.posts.forEach(item => displayNewMessage(item));
                
            }

            function requestLogHistory(){
                    var req = new XMLHttpRequest();
                    req.addEventListener("load", debugLogListener);
                    req.open("GET", "/debugHistory");
                    req.send();
                }
            
            if (!!window.EventSource) {
                var source = new EventSource('/events');

                source.addEventListener('open', function(e) {
                    console.log("Events Connected");
                    
                }, false);

                source.addEventListener('error', function(e) {
                    if (e.target.readyState != EventSource.OPEN) {
                    console.log("Events Disconnected");
                    }
                }, false);

                source.addEventListener('refreshPage', function(data) {
                    location.reload(); 
                }, false);
            }
            
            requestMessageHistory();
        </script>
    </body>
</html>
<style>
    .message-card{
        border-radius: 10px;
        border-style: solid;
        border-color: lightgrey;
        border-width: 2px;
        padding: 0 3vw 1vh;
        margin: 2vw;
        overflow:auto;
    }
    #message-container{
        width: 95%;
        margin: 10vh auto;
    }
    .time{
        margin-right: 3vw;
        float: right;
        color: gray;
    }
    body{
        font-family: sans-serif;
        font-size: .9em;
    }
</style>
)=====";
#endif