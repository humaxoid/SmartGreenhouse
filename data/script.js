// ===================== Датчики ===============================

 setInterval(function() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("IN1").innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", "/IN1", true);
        xhttp.send();
    }, 5000);

    setInterval(function() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("IN2").innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", "/IN2", true);
        xhttp.send();
    }, 5000);

    setInterval(function() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("IN3").innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", "/IN3", true);
        xhttp.send();
    }, 5000);

    setInterval(function() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("IN4").innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", "/IN4", true);
        xhttp.send();
    }, 5000);

    setInterval(function() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("IN5").innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", "/IN5", true);
        xhttp.send();
    }, 5000);

    setInterval(function() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("IN6").innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", "/IN6", true);
        xhttp.send();
    }, 5000);

    setInterval(function() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("IN7").innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", "/IN7", true);
        xhttp.send();
    }, 5000);

    setInterval(function() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
                document.getElementById("IN8").innerHTML = this.responseText;
            }
        };
        xhttp.open("GET", "/IN8", true);
        xhttp.send();
    }, 5000);
	
// ======================== Кнопки =====================================

  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
  }
  
  function onOpen(event) {
    console.log('Connection opened');
  }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
 
  function onMessage(event) { 
    switch(event.data) {
      case '0': document.getElementById("state1").innerHTML = "OFF"; document.getElementById('button1').style.backgroundColor = "#c90411"; break
      case '1': document.getElementById("state1").innerHTML = " ON "; document.getElementById('button1').style.backgroundColor = "#04b50a"; break
      case '2': document.getElementById("state2").innerHTML = "OFF"; document.getElementById('button2').style.backgroundColor = "#c90411"; break
      case '3': document.getElementById("state2").innerHTML = " ON "; document.getElementById('button2').style.backgroundColor = "#04b50a"; break
      case '4': document.getElementById("state3").innerHTML = "OFF"; document.getElementById('button3').style.backgroundColor = "#c90411"; break
      case '5': document.getElementById("state3").innerHTML = " ON "; document.getElementById('button3').style.backgroundColor = "#04b50a"; break
	  case '6': document.getElementById("state4").innerHTML = "OFF"; document.getElementById('button4').style.backgroundColor = "#c90411"; break
      case '7': document.getElementById("state4").innerHTML = " ON "; document.getElementById('button4').style.backgroundColor = "#04b50a"; break
	  case '8': document.getElementById("state5").innerHTML = "OFF"; document.getElementById('button5').style.backgroundColor = "#c90411"; break
      case '9': document.getElementById("state5").innerHTML = " ON "; document.getElementById('button5').style.backgroundColor = "#04b50a"; break
    }
    }
  
  function onLoad(event) {initWebSocket();initButton();}

  function initButton() {
    document.getElementById('button1').addEventListener('click', toggle1);
	document.getElementById('button2').addEventListener('click', toggle2);
	document.getElementById('button3').addEventListener('click', toggle3);
	document.getElementById('button4').addEventListener('click', toggle4);
	document.getElementById('button5').addEventListener('click', toggle5);
  }
  
  function toggle1(){websocket.send('toggle1');}
  function toggle2(){websocket.send('toggle2');}
  function toggle3(){websocket.send('toggle3');}
  function toggle4(){websocket.send('toggle4');}
  function toggle5(){websocket.send('toggle5');}