## Discussion

### 1. Server vs. Wand Confidence
- **Observation**  
  - The wand’s local inference confidence can be low and fluctuate significantly (e.g. dropping from 78.8% down to 55.9%).  
  - The server’s confidence tends to be higher and more stable (e.g. around 50% or above).
- **Hypothesized Reason**  
  - The cloud model is typically larger, trained on more aggregated data, and runs on more powerful hardware, resulting in more calibrated and confident predictions.  
  - The ESP32’s on-device model is compressed and runs with limited compute, so its confidence scores are more conservative and sensitive to noise.

### 2. Data Flow Diagram

```plaintext
[MPU6050 Sensor]
    ↓ (x, y, z samples @100 Hz)
[ESP32 Capture Buffer]
    ↓ (151 samples × 3 features)
[ESP32 run_classifier()]
    ├─ if confidence ≥ 80% → LED pattern (edge inference)
    └─ else → HTTP POST JSON → 
[Flask Server - POST /predict]
    ↓ parse JSON → model.predict() → return JSON
[ESP32 HTTP response]
    ↓ LED pattern (cloud inference)
```

### 3. Edge-First, Fallback-to-Server Pros & Cons

| Aspect                    | Pros                                                      | Cons                                                           |
|---------------------------|-----------------------------------------------------------|----------------------------------------------------------------|
| **Connectivity Dependence** | • Works offline for high-confidence cases<br>• Reduces cloud traffic | • Requires network for low-confidence cases, may fail if offline |
| **Latency**               | • Local inference is very fast (<100 ms)                  | • Cloud round-trip adds 200–500 ms, may hurt interactivity      |
| **Prediction Consistency** | • Deterministic on-device results                         | • Two different models can disagree, confusing user experience  |
| **Data Privacy**          | • Raw sensor data stays on device for most cases          | • Raw data is sent to cloud when offloading, raising privacy concerns |

### 4. Mitigation Strategy
- **Reduce Network Reliance**:  
  Implement **offline request queuing**. If the HTTP POST fails, store the feature payload in local flash (NVS) and retry when connectivity resumes. This ensures no data is lost during temporary network outages and maintains seamless inference behavior.
