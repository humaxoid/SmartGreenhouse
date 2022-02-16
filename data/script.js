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

  var gateway = `ws://${window.location.hostname}/ws`; // Шлюз-это точка входа в интерфейс WebSocket, возвращает текущий адрес страницы (IP-адрес веб-сервера).
  var websocket;                                       // Новая глобальная переменная под названием websocket
  window.addEventListener('load', onLoad);             // Добавьте прослушиватель событий, который будет вызывать функцию onload при загрузке веб-страницы.
  
/* инициализирует соединение WebSocket на шлюзе, определенном ранее. Мы также назначаем 
несколько функций обратного вызова при открытии, закрытии соединения WebSocket или 
получении сообщения.*/
  function initWebSocket() {
    console.log('Попытка открыть соединение с веб-сокетом...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
  }
 
/* Когда соединение открыто, мы просто печатаем сообщение в консоли и отправляем сообщение 
со словами “привет”. ESP32 получает это сообщение, поэтому мы знаем, что соединение было 
инициализировано. */ 
  function onOpen(event) {
    console.log('Соединение открыто');
  }
 
/* Если по какой-либо причине соединение с веб-сокетом закрыто, мы снова вызываем функцию 
initWebSocket() через 2000 миллисекунд (2 секунды).*/ 
  function onClose(event) {
    console.log('Соединение закрыто');
    setTimeout(initWebSocket, 2000);
  }
 
/* Наконец, нам нужно разобраться с тем, что происходит, когда мы получаем новое сообщение. 
Сервер (плата ESP) отправит сообщение "1" или “0". В соответствии с полученным сообщением мы 
хотим отобразить сообщение "ON" или "OFF" в абзаце, в котором отображается состояние. 
Помните тот тег с идентификатором=”state”? Мы получим этот элемент и установим его значение в 
положение ON или OFF.*/
  function onMessage(event) {
    switch(event.data)
    {
      case '0': document.getElementById("state1").innerHTML = "OFF";  document.getElementById('button1').style.backgroundColor = "#c90411"; document.getElementById("winimg").src = "https://kzpm.org/img/picture_main/down.gif"; break
      case '1': document.getElementById("state1").innerHTML = "ON &nbsp;"; document.getElementById('button1').style.backgroundColor = "#04b50a"; document.getElementById("winimg").src = "https://kzpm.org/img/picture_main/up.gif"; break 
      case '2': document.getElementById("state2").innerHTML = "OFF";  document.getElementById('button2').style.backgroundColor = "#c90411"; break
      case '3': document.getElementById("state2").innerHTML = "ON &nbsp;"; document.getElementById('button2').style.backgroundColor = "#04b50a"; break
      case '4': document.getElementById("state3").innerHTML = "OFF";  document.getElementById('button3').style.backgroundColor = "#c90411"; break
      case '5': document.getElementById("state3").innerHTML = "ON &nbsp;"; document.getElementById('button3').style.backgroundColor = "#04b50a"; break
	    case '6': document.getElementById("state4").innerHTML = "OFF";  document.getElementById('button4').style.backgroundColor = "#c90411"; break
      case '7': document.getElementById("state4").innerHTML = "ON &nbsp;"; document.getElementById('button4').style.backgroundColor = "#04b50a"; break
	    case '8': document.getElementById("state5").innerHTML = "OFF";  document.getElementById('button5').style.backgroundColor = "#c90411"; break
      case '9': document.getElementById("state5").innerHTML = "ON &nbsp;"; document.getElementById('button5').style.backgroundColor = "#04b50a"; break
      case '10': document.getElementById("wether").src = "https://kzpm.org/img/picture_main/pic1.gif"; break
      case '11': document.getElementById("wether").src = "https://kzpm.org/img/picture_main/pic2.gif"; break 
    }
  }
  
  function onLoad(event) {initWebSocket(); initButton();}

/* Функция initButton() получает кнопку по ее идентификатору (button1) и добавляет прослушиватель 
событий типа "click". Это означает, что при нажатии на кнопку вызывается функция переключения.*/
  function initButton() {
  document.getElementById('button1').addEventListener('click', toggle1);
	document.getElementById('button2').addEventListener('click', toggle2);
	document.getElementById('button3').addEventListener('click', toggle3);
	document.getElementById('button4').addEventListener('click', toggle4);
	document.getElementById('button5').addEventListener('click', toggle5);
  }

// Функция переключения отправляет сообщение с помощью подключения WebSocket с текстом "toggle1, toggle2 и.т.д"  
  function toggle1(){websocket.send('toggle1');}
  function toggle2(){websocket.send('toggle2');}
  function toggle3(){websocket.send('toggle3');}
  function toggle4(){websocket.send('toggle4');}
  function toggle5(){websocket.send('toggle5');}