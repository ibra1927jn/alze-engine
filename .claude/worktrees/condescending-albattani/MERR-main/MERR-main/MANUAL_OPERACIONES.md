# 📘 Manual de Operaciones - HarvestPro NZ

**Para Team Leaders y Personal de Campo**  
**Versión 1.0** | Febrero 2026

---

## 🎯 Introducción

Este manual explica cómo usar las nuevas funciones de HarvestPro NZ para garantizar:
- ✅ **Protección legal** (auditoría automática)
- ✅ **Continuidad operativa** (trabajo sin conexión)
- ✅ **Entrenamiento seguro** (modo simulación)

---

## 1️⃣ Monitor de Sincronización (Trabajo Offline)

### ¿Qué es?
Cuando no hay señal de internet en el huerto, HarvestPro **guarda todos los datos localmente** y los sincroniza automáticamente cuando vuelve la conexión.

### ¿Cómo saber si mis datos están seguros?

#### En la Pantalla del Runner (Recolector)
Busca el **banner naranja** en la parte superior:

```
🔄 Syncing 15 items...
```

**Esto significa:**
- ✅ Tienes **15 cubetas** esperando sincronización
- ✅ Están **guardadas de forma segura** en tu dispositivo
- ✅ Se subirán automáticamente cuando haya internet

#### En el Dashboard del Manager
Mira la sección **"Sync Monitor"**:

- **🟢 Verde** = Todo sincronizado
- **🟡 Amarillo** = Sincronizando (normal)
- **🔴 Rojo** = Muchos datos pendientes (>50 cubetas)

### ¿Qué hacer si hay muchos datos pendientes?

**Opción 1 - Esperar**:
- El sistema sincroniza **automáticamente** cada 30 segundos
- No hagas nada, solo espera

**Opción 2 - Forzar sincronización**:
1. Lleva el dispositivo a una zona con WiFi
2. El sistema sincronizará en menos de 1 minuto

### ⚠️ IMPORTANTE: ¿Puedo perder datos?

**NO** ❌  
- Los datos están **100% seguros** aunque el dispositivo se apague
- Se guardan en el almacenamiento interno del dispositivo
- Solo se borran **después de confirmarse** en el servidor

### Ejemplo Práctico

```
ESCENARIO: Estás en una fila del huerto sin señal

1. Escaneas 10 cubetas → ✅ Guardadas localmente
2. Sales del huerto → 🔄 Aparece "Syncing 10 items..."
3. Después de 30 segundos → ✅ "All synced"
```

---

## 2️⃣ Alertas de Riesgo de Salario Mínimo

### ¿Qué es?
El **Wage Shield** detecta automáticamente cuando un recolector está en riesgo de ganar menos del salario mínimo legal.

### ¿Dónde lo veo?

#### En el Dashboard (Manager)
Panel **"Wage Shield"** muestra:

```
⚠ 2 workers at risk
└─ María G. → $21.8/hr (below $23.50 minimum)
└─ Carlos M. → $22.1/hr (below $23.50 minimum)
```

#### En el HeatMap
Los recolectores en riesgo aparecen con **borde rojo** alrededor de su nombre.

### ¿Qué hacer ante una alerta?

#### Paso 1: Verificar Velocidad
Revisa si el recolector está trabajando **lento** comparado con su promedio:
- Ve a la vista **"Teams"**
- Busca su nombre
- Verifica **"Velocity (buckets/hr)"**

#### Paso 2: Actuar

**Si es lento por experiencia**:
- Asígnalo a **filas más fáciles** (Row 1-5)
- Dale más **entrenamiento**
- Considera **pago garantizado** por el día

**Si es lento por cansancio**:
- Recomienda un **descanso de 15 min**
- Ofrece **agua y sombra**
- Verifica **horas trabajadas** (no exceder límites)

**Si es lento porque la fruta está escasa**:
- **Mueve todo el equipo** a otra sección
- Reporta al Manager para **ajuste de bucket rate**

### Ejemplo Práctico

```
ESCENARIO: Alerta en Wage Shield

Dashboard muestra:
⚠ Juan P. → $22.5/hr (below $23.50)

ACCIÓN:
1. Verificas: Juan lleva 4 hrs, solo 18 buckets
2. Hablas con Juan → Dice que Row 12 está vacía
3. Mueves a Juan a Row 3 (más fruta)
4. Después de 1 hr → Juan sube a $24.8/hr ✅
```

---

## 3️⃣ Modo Simulación (Entrenamiento)

### ¿Qué es?
El **Simulation Mode** permite **entrenar nuevos Runners** sin afectar los datos reales de producción.

### ¿Cómo activarlo?

#### Opción 1: Desde el simulador web

1. Abre el archivo: `scripts/drill-runner.html`
2. Elige un **escenario de entrenamiento**:
   - **Scenario 1**: Equipo alta productividad
   - **Scenario 2**: Alguien bajo mínimo
   - **Scenario 3**: Faltas de descanso
   - **Scenario 5**: Equipo mixto (recomendado)
3. Click **"Run Scenario"**
4. Ve al **Dashboard** → Verás banner **"🧪 SIMULATION MODE"**

#### Opción 2: Manualmente (para testing)

1. Abre la **consola del navegador** (F12)
2. Escribe:
```javascript
// Activar simulación
localStorage.setItem('simulation_mode', 'true');

// Cargar datos de prueba
localStorage.setItem('simulated_pickers', JSON.stringify([...]));

// Recargar página
location.reload();
```

### ¿Qué puedo hacer en Simulation Mode?

✅ **Ver recolectores simulados** en el dashboard  
✅ **Calcular payroll** con datos de prueba  
✅ **Entrenar a Team Leaders** en el HeatMap  
✅ **Probar alertas de compliance**  

❌ **NO se guardan en Supabase**  
❌ **NO afectan datos reales**  
❌ **NO generan reportes oficiales**

### ¿Cómo desactivar Simulation Mode?

**Método 1 - Automático**:
- Cierra el navegador
- Vuelve a abrir → Modo normal

**Método 2 - Manual**:
1. Abre consola (F12)
2. Escribe:
```javascript
localStorage.removeItem('simulation_mode');
localStorage.removeItem('simulated_pickers');
location.reload();
```

### Ejemplo de Uso: Entrenar nuevo Team Leader

```
OBJETIVO: Enseñar a reconocer riesgo de salario mínimo

PASOS:
1. Abrir drill-runner.html
2. Seleccionar "Scenario 2: Below Minimum"
3. Click "Run Scenario"
4. Mostrar al nuevo TL el dashboard:
   → "Mira, esta alerta roja significa que Ana está bajo mínimo"
   → "¿Qué harías en esta situación?"
5. Dejar que practique asignando filas
6. Cerrar navegador para volver a datos reales
```

---

## 4️⃣ Sistema de Auditoría Legal (Wage Shield)

### ¿Qué es?
Cada vez que cambias **configuraciones críticas** (precio de bucket, salario mínimo), el sistema **guarda un registro automático** con:
- 📅 Fecha y hora exacta
- 👤 Quién hizo el cambio
- 📊 Valor anterior y nuevo

### ¿Qué cambios se registran?

✅ **Bucket Rate** ($6.50 → $7.00)  
✅ **Salario Mínimo** ($23.15 → $23.50)  
✅ **Daily Target** (40 → 45 toneladas)  

### ¿Dónde se guardan los registros?

**En Supabase** → Tabla `harvest_logs` o `audit_logs`

### ¿Cómo verificar que funciona?

#### Paso 1: Hacer un cambio
1. Ir a **Manager Dashboard**
2. Abrir **Settings Modal** (ícono lápiz)
3. Cambiar **Bucket Rate** (ejemplo: $7.00 → $7.50)
4. Click **"Save Settings"**

#### Paso 2: Verificar en consola
1. Abrir **consola del navegador** (F12)
2. Buscar mensaje:
```
🟢 [Audit] settings.day_setup_modified
Old: { bucketRate: 7.00 }
New: { bucketRate: 7.50 }
```

#### Paso 3: Verificar en Supabase
Ejecutar query:
```sql
SELECT 
  event_type,
  old_value,
  new_value,
  user_id,
  created_at
FROM audit_logs
WHERE event_type = 'settings.day_setup_modified'
ORDER BY created_at DESC
LIMIT 5;
```

### ¿Por qué es importante?

En caso de **inspección laboral**, puedes demostrar:
- ✅ **Cuándo** se cambió el bucket rate
- ✅ **Quién** autorizó el cambio
- ✅ **Historial completo** de ajustes salariales

### Ejemplo Práctico

```
ESCENARIO: Inspección del Ministerio de Trabajo

Inspector pregunta:
"¿Desde cuándo pagan $7.50 por bucket?"

RESPUESTA (con audit logs):
1. Abres Supabase
2. Ejecutas query de audit_logs
3. Muestras registro:
   └─ Fecha: 2026-02-08 14:30:00
   └─ Usuario: manager@harvestpro.nz
   └─ Cambio: $6.50 → $7.50
4. Inspector confirma cumplimiento ✅
```

---

## 5️⃣ Indicadores del Dashboard

### 🔍 Qué Significan

| Indicador | Qué significa | Acción requerida |
|-----------|---------------|------------------|
| **Production: 10 buckets** | Total escaneado hoy | Ninguna (informativo) |
| **Est. Cost: $376 NZD** | Costo estimado del día | Ninguna (informativo) |
| **Active Crew: 0 pickers** | Recolectores activos HOY | Si es 0 → Asignar equipo |
| **Daily Target: 0% Complete** | Progreso hacia meta | Si bajo → Motivar equipo |
| **⚠ 2 workers at risk** | Bajo salario mínimo | **Revisar y actuar** |
| **🔄 Syncing 15 items** | Datos pendientes | Esperar o buscar WiFi |

### 🚨 Alertas Críticas

**ROJO**: Requiere acción inmediata
- ⚠️ **Wage risk** → Asignar mejores filas
- 🔴 **>50 pending** → Buscar conexión WiFi

**AMARILLO**: Monitorear
- 🟡 **Syncing data** → Normal (esperar)
- 🟡 **Low velocity** → Verificar si es nuevo

**VERDE**: Todo bien
- ✅ **All synced** → Datos guardados
- ✅ **Above minimum** → Cumplimiento OK

---

## 6️⃣ Preguntas Frecuentes (FAQ)

### P: ¿Qué pasa si se va la luz del dispositivo?
**R**: ✅ Los datos están seguros. Se guardan inmediatamente después de cada escaneo.

### P: ¿Puedo usar el sistema sin internet?
**R**: ✅ Sí. El sistema funciona 100% offline. Sincroniza cuando vuelva la señal.

### P: ¿Cómo sé si un dato se guardó?
**R**: Si escaneaste el código QR y apareció en pantalla, está guardado.

### P: ¿El modo simulación afecta mis datos reales?
**R**: ❌ No. Los datos simulados están completamente separados.

### P: ¿Cada cuánto sincroniza automáticamente?
**R**: Cada **30 segundos** cuando hay conexión.

### P: ¿Puedo borrar datos pendientes de sincronización?
**R**: ❌ No se recomienda. Espera a que sincronicen automáticamente.

### P: ¿Qué hago si un recolector está bajo el mínimo?
**R**: Ver **Sección 2: Alertas de Riesgo**. Resumen: Mejor fila o pago garantizado.

### P: ¿Cómo entreno a nuevos Team Leaders?
**R**: Usa **Modo Simulación** (Sección 3) con Scenario 5.

---

## 7️⃣ Contacto y Soporte

### Problemas Técnicos
- **Email**: support@harvestpro.nz
- **Teléfono**: +64 XX XXX XXXX

### Reportar Bug
1. Captura **screenshot** del error
2. Anota **qué estabas haciendo** cuando ocurrió
3. Envía a support con asunto: `[BUG] Descripción breve`

### Solicitar Nueva Función
- Email: features@harvestpro.nz
- Describe **qué necesitas** y **por qué**

---

## 📝 Checklist Diario (Team Leader)

**Al inicio del día**:
- [ ] Verificar que todos los dispositivos están cargados
- [ ] Confirmar conexión WiFi en zona de salida
- [ ] Revisar equipo asignado en Dashboard

**Durante el día**:
- [ ] Monitorear Wage Shield cada 2 horas
- [ ] Verificar sync status si hay problemas de señal
- [ ] Actuar ante alertas rojas inmediatamente

**Al final del día**:
- [ ] Confirmar que "Syncing status" = ✅ "All synced"
- [ ] Revisar total de producción vs. meta
- [ ] Reportar incidencias al Manager

---

**¡Listo!** Ahora tienes todo lo necesario para operar HarvestPro NZ con confianza. 🚀

_Para más detalles técnicos, consulta los reportes de validación en la carpeta `evidencias/`._
