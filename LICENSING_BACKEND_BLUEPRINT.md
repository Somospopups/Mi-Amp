# MI AMP — Blueprint Completo del Sistema de Licencias Automático (Self-Hosted)

> **Propósito de este documento:** Darle a cualquier agente o desarrollador (incluso en un chat nuevo sin historial previo) **todo el contexto técnico y el plan paso a paso exacto** para implementar el sistema comercial de licencias automáticas de **MI AMP** (VST3 / AU / Standalone) de **Popups Audio**.

---

## 1. Contexto Actual del Proyecto MI AMP

- **Producto:** `MI AMP` (Company: `Popups Audio`, Bundle ID: `com.popupsaudio.miamp`, Manufacturer Code: `PpAu`, Plugin Code: `MiAm`).
- **Repositorio:** `https://github.com/Somospopups/Mi-Amp` (rama `main`, CI/CD en `.github/workflows/build_windows.yml` que compila **Windows x64 VST3 + Standalone** y **macOS Universal arm64/x86_64 AU + VST3 + Standalone**).
- **Framework:** C++17 + **JUCE 8.0.4** (`CMakeLists.txt`).
- **Reglas visuales obligatorias (`UI_RULES.md` + `Source/MiAmpGlassTheme.h`):**
  - Todos los modales, popovers y overlays deben usar el tema único **Vidrio: Equilibrado (Legible)**:
    - `miamp_theme::paintFrostedGlassPanel(...)`
    - `miamp_theme::paintGlassSubPanel(...)`
    - `miamp_theme::paintGlassCard(...)`
  - **Prohibido** usar la palabra `"zenbox"` en cualquier archivo o UI. La marca es **MI AMP**.
  - `PluginEditor.cpp` usa un escenario virtual `1550 × 768` (`stageScale()`) y `GlassLookAndFeel`.
- **Rutas de almacenamiento local actuales (`Source/PluginProcessor.cpp`):**
  - `getSettingsFile()` guarda configuración en:
    `juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("PopupsAudio").getChildFile("MiAmp")`
  - En Windows: `%APPDATA%\PopupsAudio\MiAmp\`
  - En macOS: `~/Library/Application Support/PopupsAudio/MiAmp/`
- **AutoUpdater actual (`Source/AutoUpdater.h`):**
  - Consulta `https://github.com/Somospopups/Mi-Amp/releases/download/v1.0.0-win/version.json`.
  - **Nota importante para comercialización:** Cuando el plugin pase a ser pago, el release/descarga de binarios **NO debe quedar libremente descargable para usuarios sin licencia**, o bien los binarios siguen pudiendo actualizarse porque al abrirse requieren una licencia válida activada en el equipo (`license.json` firmado).

---

## 2. Arquitectura Recomendada del Sistema de Licencias

### Objetivos clave
1. **100% Automático:** El cliente paga → el webhook de la pasarela crea el serial único → se envía automáticamente por email al cliente al instante (sin intervención manual).
2. **Self-Hosted (sin comisiones extra por licencia de terceros):** Backend liviano propio (Node.js + Express/Fastify + SQLite o PostgreSQL, desplegable en Railway, Render, Fly.io o VPS con HTTPS).
3. **Activación por Machine ID (anti-piratería realista):** Cada serial permite activar hasta `N` máquinas (ej. **2 dispositivos**: estudio + notebook), vinculando cada activación a un hash estable del hardware. Incluye opción de desactivar una máquina desde el plugin o desde soporte para liberar un slot.
4. **Token Offline Firmado (Ed25519):**
   - Una vez activado con internet, el servidor entrega un **certificado/token offline firmado con la clave privada Ed25519** del servidor.
   - El plugin (C++) contiene únicamente la **clave pública Ed25519** embebida (`32 bytes`) y verifica la firma **100% offline** al instante en cada arranque (sin depender de internet en vivo en el estudio o en un show!).
   - Cero bloqueo en el hilo de audio (`processBlock()`): la verificación ocurre al construir el `AudioProcessor` / abrir el editor y solo escribe un `std::atomic<bool> licenseValid`.

---

## 3. Paso 1 — Criptografía Asimétrica (Ed25519)

**Por qué Ed25519 y NO HMAC ni validación simple por string:**
- Un serial estilo `MIAMP-XXXX-XXXX-XXXX-XXXX` se valida contra la base de datos del servidor (no por algoritmo reversible en el cliente, porque los keygens rompen eso en minutos).
- Cuando el servidor aprueba el serial + `machine_id`, firma un payload JSON con **Ed25519 Private Key** (que NUNCA sale del servidor).
- El plugin solo tiene la **Ed25519 Public Key** (`32 bytes`) para verificar la firma del `license.lic` local. Sin la clave privada del servidor es matemáticamente imposible falsificar un token válido.

### Generación de claves (una única vez, en entorno seguro)
En Node.js (`crypto` nativo, Node 18+):
```js
const { generateKeyPairSync } = require('crypto');
const { publicKey, privateKey } = generateKeyPairSync('ed25519');
// Exportar PEM/DER o raw 32 bytes hex para embeber la pública en C++
```
- `ED25519_PRIVATE_KEY` → variable de entorno secreta en el servidor (NUNCA subirla a GitHub).
- `kEd25519PublicKey[32]` → constante en `Source/Licensing/LicenseVerifier.h` dentro del C++.

### Implementación de verificación Ed25519 en C++ sin dependencias pesadas
- Usar **`TweetNaCl`** (`tweetnacl.c/.h`, dominio público, ~700 líneas) o **`monocypher`** (`monocypher.c/.h`, 2 archivos, sin dependencias externas, compila limpio en MSVC Windows y Clang macOS arm64/x86_64).
- **Recomendación:** `monocypher` (`crypto_ed25519_check(...)`) agregado en `CMakeLists.txt` dentro de `target_sources(MiAmp PRIVATE ...)`.

---

## 4. Paso 2 — Base de Datos del Backend (Esquema SQL)

Se recomienda **PostgreSQL** (Supabase / Neon / Railway) o **SQLite** (con volumen persistente). Tablas mínimas:

### Tabla `licenses`
| Columna | Tipo | Descripción |
|---|---|---|
| `id` | UUID / SERIAL PK | Identificador interno |
| `serial_key` | TEXT UNIQUE NOT NULL | Ej: `MIAMP-8F3K-92LP-Q7VX-4M2B` (alfanumérico sin `0/O/1/I` para evitar errores al tipear) |
| `customer_email` | TEXT NOT NULL | Email del comprador |
| `customer_name` | TEXT | Nombre del comprador |
| `product_sku` | TEXT NOT NULL DEFAULT `'miamp-full'` | Producto comprado |
| `payment_provider` | TEXT NOT NULL | `'mercadopago'`, `'stripe'`, `'lemonsqueezy'`, `'manual'` |
| `payment_id` | TEXT UNIQUE NOT NULL | ID único de transacción (evita duplicar seriales si el webhook se reintenta = **idempotencia**) |
| `max_activations` | INTEGER NOT NULL DEFAULT 2 | Máximo de PCs/Macs simultáneas |
| `status` | TEXT NOT NULL DEFAULT `'active'` | `'active'`, `'revoked'`, `'refunded'` |
| `created_at` | TIMESTAMP DEFAULT NOW() | Fecha de compra |

### Tabla `activations`
| Columna | Tipo | Descripción |
|---|---|---|
| `id` | UUID / SERIAL PK | ID de activación |
| `license_id` | FK → `licenses.id` | Licencia asociada |
| `machine_id` | TEXT NOT NULL | SHA-256 del hardware ID enviado por el plugin |
| `machine_name` | TEXT | Ej: `DESKTOP-STUDIO (Windows)` o `MacBook-Pro (macOS)` |
| `os_platform` | TEXT | `'windows'` o `'macos'` |
| `plugin_version` | TEXT | Ej: `'1.1.34'` |
| `last_seen_at` | TIMESTAMP | Última vez que chequeó online |
| `is_active` | BOOLEAN DEFAULT TRUE | `false` si fue desactivada |
| **Constraint** | `UNIQUE(license_id, machine_id)` | Reactivar la misma PC reutiliza su slot sin gastar otro |

---

## 5. Paso 3 — API REST del Backend (Endpoints)

Base URL ejemplo: `https://api.miampaudio.com` (siempre HTTPS).

### 1. `POST /webhooks/mercadopago` y `POST /webhooks/stripe` (Generación automática al pagar)
1. **Verificar firma criptográfica del webhook** (`x-signature` de Mercado Pago / `stripe-signature` de Stripe). Rechazar con `401` si no es válida.
2. Verificar que el pago esté aprobado (`status === 'approved'` / `payment_status === 'paid'`).
3. **Idempotencia:** Buscar si `payment_id` ya existe en `licenses`. Si ya existe, responder `200 OK` sin crear otro serial.
4. Generar serial criptográficamente aleatorio (`crypto.randomBytes`):
   - Alfabeto limpio: `ABCDEFGHJKLMNPQRSTUVWXYZ23456789` (32 caracteres, sin `0/O/1/I`).
   - Formato: `MIAMP-XXXX-XXXX-XXXX-XXXX`.
5. Insertar fila en `licenses` con `max_activations = 2`, `status = 'active'`.
6. **Enviar email automático transaccional** (Resend / Postmark / Brevo / SendGrid) con:
   - El serial `MIAMP-XXXX-XXXX-XXXX-XXXX`
   - Links de descarga del instalador Windows (`.exe`) y macOS (`.pkg`/`.dmg`)
   - Instrucciones rápidas de activación.
7. **Si llega un webhook de reembolso/contracargo (`charge.refunded` / `payment.refunded`):** cambiar `licenses.status = 'refunded'` para que futuras activaciones/revalidaciones fallen automáticamente.

### 2. `POST /api/v1/activate` (Llamado desde el plugin C++)
- **Request JSON:**
  ```json
  {
    "serial": "MIAMP-8F3K-92LP-Q7VX-4M2B",
    "machine_id": "sha256_hex_del_hardware",
    "machine_name": "Studio-PC",
    "os": "windows",
    "version": "1.1.34"
  }
  ```
- **Lógica:**
  1. Normalizar serial (`trim().toUpperCase()`). Buscar en `licenses`.
  2. Si no existe → `404 { "error": "invalid_serial" }`.
  3. Si `status !== 'active'` → `403 { "error": "license_revoked" }`.
  4. Contar filas en `activations` donde `license_id = lic.id` y `is_active = true`.
  5. Si `machine_id` ya existe en `activations` para esa licencia: reactivar (`is_active = true`, actualizar `last_seen_at`).
  6. Si no existe y `active_count >= lic.max_activations` → `409 { "error": "max_activations_reached", "max": 2 }`.
  7. Si hay lugar → insertar nueva fila en `activations`.
  8. Construir payload de token offline:
     ```json
     {
       "v": 1,
       "product": "miamp",
       "serial_hint": "MIAMP-****-****-****-4M2B",
       "license_id": "uuid",
       "machine_id": "sha256_hex_del_hardware",
       "issued_at": 1795000000,
       "grace_days": 30
     }
     ```
  9. Firmar el JSON serializado canónicamente (en UTF-8 sin espacios extra o Base64url de `payload_bytes`) con `ED25519_PRIVATE_KEY` → `signature_b64`.
- **Response `200 OK`:**
  ```json
  {
    "ok": true,
    "token_payload_b64": "...",
    "signature_b64": "...",
    "activations_used": 1,
    "max_activations": 2
  }
  ```

### 3. `POST /api/v1/deactivate` (Liberar máquina desde el plugin)
- **Request:** `{ "serial": "...", "machine_id": "..." }`
- Marca `is_active = false` en `activations` para liberar ese slot y permitir activar otra computadora.
- El plugin borra su archivo local `license.lic` tras confirmación (o incluso si quiere desactivar localmente).

### 4. `POST /api/v1/refresh` (Renovación silenciosa en background cuando hay internet)
- **Request:** `{ "license_id": "...", "machine_id": "..." }`
- Verifica que `licenses.status === 'active'` y `activations.is_active === true`.
- Si sigue activa → devuelve un token firmado actualizado con nuevo `issued_at`.
- Si fue revocada/reembolsada/desactivada remotamente → devuelve `403 { "error": "revoked" }` y el plugin revoca `license.lic` localmente.

### 5. Endpoints de Admin (protegidos con `Authorization: Bearer ADMIN_API_TOKEN`)
- `POST /api/admin/licenses/create` — Generar seriales manualmente (cortesía, beta testers, YouTubers, prensa).
- `POST /api/admin/licenses/revoke` — Revocar serial o liberar máquinas de un cliente que formateó su PC sin desactivar.
- `GET /api/admin/licenses/search?q=...` — Buscar por email o serial.

---

## 6. Paso 4 — Machine ID Estable en C++ (Windows + macOS)

Debe ser **determinista, único por máquina y resistente a cambios menores** (no usar IP ni fecha; en JUCE tampoco usar solo `SystemStats::getDeviceIdentifiers()` sin testear porque puede variar si no está disponible):

- **Windows:** Leer `MachineGuid` del Registro (`HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography`, valor `MachineGuid`) usando `juce::WindowsRegistry::getValue` + nombre de volumen del sistema o CPU string (`juce::SystemStats::getCpuVendor()`), y calcular **SHA-256** con `juce::SHA256`.
- **macOS:** Leer `IOPlatformUUID` vía `ioreg -rd1 -c IOPlatformExpertDevice` (o `IOKit` directamente en C++/`IORegistryEntryCreateCFProperty` de `IOPlatformExpertDevice`, sin subproceso!) + `kIOPlatformSerialNumberKey` y calcular **SHA-256**.
- **Fallback multiplataforma:** `juce::SystemStats::getUniqueDeviceID()` / `getDeviceIdentifiers()` si alguna lectura nativa fallara, siempre normalizado con SHA-256 hex.

---

## 7. Paso 5 — Integración C++ dentro de `Mi-Amp`

### Archivos nuevos propuestos en `Source/Licensing/`:
1. `Source/Licensing/monocypher.h` y `Source/Licensing/monocypher.c` (Ed25519 + SHA-256).
2. `Source/Licensing/LicenseManager.h` y `Source/Licensing/LicenseManager.cpp`:
   - `getMachineIdHash()`: obtiene y cachea el SHA-256 estable de la máquina.
   - `getLicenseFile()`: `%APPDATA%\PopupsAudio\MiAmp\license.lic` (Windows) / `~/Library/Application Support/PopupsAudio/MiAmp/license.lic` (macOS).
   - `loadAndVerifyLocalLicense()`:
     1. Lee `license.lic` (`{ "payload_b64": "...", "signature_b64": "..." }`).
     2. Verifica firma Ed25519 con la `kPublicKey[32]` embebida.
     3. Decodifica payload y verifica `product == "miamp"` y `machine_id == getMachineIdHash()`.
     4. Actualiza `std::atomic<bool> licensed { true }` (en `MiAmpAudioProcessor`).
     5. Lanza una única revalidación silenciosa en hilo separado (`juce::Thread`) contra `/api/v1/refresh` si hay internet y pasaron > 3 días desde `issued_at` (si no hay internet, **sigue funcionando offline**; opcionalmente podés permitir offline ilimitado o refresco cada X días sin bloquear al músico en vivo).
   - `activateOnlineAsync(serial, callback)`: llama a `POST /api/v1/activate` usando `juce::URL` con HTTPS en un `juce::Thread`, guarda `license.lic` en disco si `ok == true`, y dispara `callback(success, errorMessage)`.
   - `deactivateOnlineAsync(callback)`: llama a `POST /api/v1/deactivate` y borra `license.lic`.

### Comportamiento en Audio (`Source/PluginProcessor.cpp`)
- **Nunca hacer I/O ni red en `processBlock()`!** Solo leer `licenseManager.isLicensedAtomic()` (`std::memory_order_relaxed`).
- **Modo Demo / Sin Licencia (elegir estrategia comercial):**
  - **Opción A (Recomendada — Muro de Activación / Trial de 7 días o Demo con silencio breve periódico):**
    - Si no tiene licencia ni trial activo: mostrar overlay de activación en `PluginEditor` y en audio silenciar o aplicar un beep/silencio suave cada 20s (o bypass completo con aviso) hasta activar.
    - Si tiene trial de 7–14 días (token trial firmado por el servidor con `"exp": timestamp`): el plugin funciona al 100% durante el trial y al vencer muestra el modal de activación.
  - **Opción B (Activación obligatoria al abrir):**
    - Al abrir por primera vez (sin `license.lic`), toda la UI queda detrás de un **modal centrado de Activación en Vidrio Equilibrado** (`MI AMP — ACTIVAR LICENCIA`), con campo para pegar el serial (`MIAMP-XXXX-XXXX-XXXX-XXXX`), botón `ACTIVAR`, link `COMPRAR LICENCIA`, y botón `PROBAR MODO DEMO` (opcional).

### Integración en UI (`Source/PluginEditor.{h,cpp}`)
- **Modal de Activación (`LicenseModal`):**
  - Mismo diseño `miamp_theme::paintFrostedGlassPanel` (`Vidrio: Equilibrado (Legible)`), centrado sobre el stage `1550 × 768` con animación suave de entrada/salida.
  - Mensajes localizados en los 5 idiomas (`Source/Localization.h`: `ES`, `EN`, `PT`, `FR`, `DE`):
    - Serial inválido
    - Límite de activaciones alcanzado (ej. `2/2 PCs usadas — desactivá una para continuar`)
    - Licencia revocada
    - Sin conexión a internet al activar
- **Sección en `CONFIG` (`GlassPopover`):**
  - Mostrar estado de licencia (`Activado — MIAMP-****-****-4M2B`) y botón **`DESACTIVAR ESTA PC`** para que el usuario pueda migrar su licencia a otra computadora sin escribir a soporte.

---

## 8. Paso 6 — CMake y Compilación Multiplataforma

En `CMakeLists.txt`:
- Agregar `Source/Licensing/monocypher.c`, `Source/Licensing/monocypher.h`, `Source/Licensing/LicenseManager.cpp`, `Source/Licensing/LicenseManager.h` en `target_sources(MiAmp PRIVATE ...)`.
- En Windows (`MSVC`), `juce::URL` usa **WinINet/WinHTTP** nativamente (soporta HTTPS sin OpenSSL externo).
- En macOS (`Clang`), `juce::URL` usa **NSURLSession** nativamente (soporta HTTPS sin OpenSSL externo) y `IOKit.framework` para `IOPlatformUUID` (`target_link_libraries(MiAmp PRIVATE "-framework IOKit")` bajo `if(APPLE)`).
- Recordar mantener `JUCE_USE_CURL=0` (como ya está en `CMakeLists.txt`) para no requerir `libcurl` en Windows/macOS.

---

## 9. Paso 7 — Despliegue del Backend (Checklist Paso a Paso)

1. **Crear proyecto backend** en una carpeta separada (ej. `mi-amp-license-server/`):
   - Stack sugerido: `Node.js 20 + Fastify/Express + better-sqlite3` (o `pg` para PostgreSQL) + `resend` (emails) + `crypto` nativo de Node (Ed25519).
2. **Generar par de llaves Ed25519**:
   - Guardar `ED25519_PRIVATE_KEY_HEX` en `.env` / secretos del servidor.
   - Copiar `kEd25519PublicKey[32]` dentro de `Source/Licensing/LicenseManager.cpp`.
3. **Configurar Pasarela de Pago**:
   - **Argentina / LATAM:** Mercado Pago (Checkout Pro / Suscripciones / Webhook `payment`) o LemonSqueezy / Paddle (Merchant of Record global, cobra en USD con tarjeta/PayPal y resuelve impuestos internacionales automáticamente).
   - **Recomendación para vender globalmente en USD + LATAM:** **LemonSqueezy** o **Stripe + Mercado Pago**, ambos disparan un webhook HTTPS hacia tu servidor al instante de aprobarse el pago.
4. **Configurar Email Transaccional**:
   - Dominio propio verificado (`SPF` + `DKIM`, ej. `licencias@popupsaudio.com`) con Resend / Postmark para que el correo con el serial **no caiga en Spam**.
5. **Subir servidor con HTTPS**:
   - Railway / Fly.io / Render / VPS con Nginx + Certbot.
   - Probar con `curl` los 4 endpoints (`activate`, `deactivate`, `refresh`, `webhook` simulado).
6. **Conectar el Plugin C++**:
   - Implementar `LicenseManager` + `LicenseModal` + botón desactivar en `CONFIG`.
   - Testear en Windows y macOS:
     1. Activación online con serial válido → crea `license.lic`.
     2. Cerrar internet → reiniciar DAW y Standalone → **abre activado 100% offline**.
     3. Intentar activar en 3ra PC con límite 2 → error claro `2/2 activaciones usadas`.
     4. Desactivar desde 1ra PC → libera slot → 3ra PC activa OK.
     5. Simular reembolso en webhook → el servidor revoca y en la próxima conexión online bloquea la licencia.

---

## 10. Resumen Rápido para el Próximo Agente

Si retomás este proyecto en un chat nuevo:
1. Leé este archivo (`LICENSING_BACKEND_BLUEPRINT.md`), `PROJECT_HANDOFF.md`, `UI_RULES.md` y `CMakeLists.txt`.
2. Respetá estrictamente el estilo **`Vidrio: Equilibrado (Legible)`** (`Source/MiAmpGlassTheme.h`) y **cero menciones a `"zenbox"`**.
3. Construí primero el servidor Node.js + DB + Ed25519 + Webhooks + Email y verificalo con tests automatizados.
4. Luego implementá `Source/Licensing/` (`monocypher` + `LicenseManager` + UI modal en `PluginEditor.cpp` + atomic gate en `PluginProcessor.cpp`) y actualizá `CMakeLists.txt` para que compile limpio tanto en **Windows MSVC** como en **macOS Universal (`arm64;x86_64`)**.
