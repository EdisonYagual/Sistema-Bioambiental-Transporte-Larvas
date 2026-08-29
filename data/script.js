// Interfaz local: funciona dentro de la red del ESP32 y no depende de Ubidots.
let socket;
let ultimoEstado = {bomba:false,peltier:false};
const $ = id => document.getElementById(id);

function aviso(texto){const t=$("toast");t.textContent=texto;t.classList.add("show");setTimeout(()=>t.classList.remove("show"),2400)}
function textoEstado(id,activo){$(id).textContent=activo?"CONECTADO":"SIN CONEXIÓN"}

function conectar(){
  socket=new WebSocket(`ws://${location.hostname}:81/`);
  socket.onopen=()=>{$("conexion").textContent="Conectado";$("conexion").className="status online"};
  socket.onclose=()=>{$("conexion").textContent="Reconectando";$("conexion").className="status offline";setTimeout(conectar,2000)};
  socket.onmessage=e=>{try{renderEstado(JSON.parse(e.data))}catch(_){aviso("Dato recibido no válido")}};
}

function renderEstado(d){
  ultimoEstado=d;
  $("temperatura").textContent=Number(d.temperatura).toFixed(1);
  $("ph").textContent=Number(d.ph).toFixed(2);
  $("bomba").checked=!!d.bomba;$("peltier").checked=!!d.peltier;
  $("modo").textContent=d.manual?"MANUAL":"AUTOMÁTICO";
  textoEstado("wifi",d.wifi);textoEstado("nube",d.nube);
}

function ordenar(accion,estado){
  if(!socket||socket.readyState!==WebSocket.OPEN){aviso("ESP32 sin conexión");renderEstado(ultimoEstado);return}
  socket.send(JSON.stringify({accion,estado}));
  aviso(`${accion} ${estado?"encendido":"apagado"}`);
}

async function cargarViaje(){
  try{
    const v=await fetch("/api/viaje").then(r=>r.json());
    $("heap").textContent=`${Math.round(v.heap/1024)} KB`;textoEstado("nube",v.nube);
    $("formViaje").classList.toggle("hidden",v.activo);$("viajeActivo").classList.toggle("hidden",!v.activo);
    $("viajeEstado").textContent=v.activo?"EN CURSO":"SIN VIAJE";$("viajeEstado").classList.toggle("neutral",!v.activo);
    if(v.activo){$("rutaViaje").textContent=`${v.origen} → ${v.destino}`;$("tanqueViaje").textContent=v.tanque||"Sin identificar";$("idViaje").textContent=v.id;$("inicioViaje").textContent=v.inicio?new Date(v.inicio*1000).toLocaleString():"Hora pendiente de Internet"}
  }catch(_){aviso("No se pudo consultar el viaje")}
}

async function post(url,datos){
  const body=new URLSearchParams(datos);
  const r=await fetch(url,{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body});
  const d=await r.json();if(!r.ok)throw new Error(d.error||"Operación rechazada");return d;
}

$("bomba").addEventListener("change",e=>ordenar("bomba",e.target.checked));
$("peltier").addEventListener("change",e=>ordenar("peltier",e.target.checked));
$("iniciarViaje").addEventListener("click",async()=>{try{await post("/api/viaje/iniciar",{origen:$("origen").value.trim(),destino:$("destino").value.trim(),tanque:$("tanque").value.trim()});aviso("Viaje iniciado");cargarViaje()}catch(e){aviso(e.message)}});
$("finalizarViaje").addEventListener("click",async()=>{if(!confirm("¿Finalizar el viaje actual?"))return;try{await post("/api/viaje/finalizar",{});aviso("Viaje finalizado");cargarViaje()}catch(e){aviso(e.message)}});

addEventListener("load",()=>{conectar();cargarViaje();setInterval(cargarViaje,15000)});
