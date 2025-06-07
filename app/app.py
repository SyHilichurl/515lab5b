from flask import Flask, request, jsonify
import numpy as np
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)

# Mock model for testing
# Replace with actual model loading if needed
def mock_predict(input_array):
    # Return a mock prediction for three classes: O, V, Z
    return np.array([[0.5, 0.3, 0.2]])

@app.route('/', methods=['GET'])
def home():
    return 'Wand Gesture API is running!'

@app.route('/predict', methods=['POST'])
def predict():
    try:
        payload = request.get_json()
        if 'data' not in payload:
            return jsonify({'error': 'No data provided'}), 400
        data = payload['data']

        # Convert to numpy array
        arr = np.array(data)

        # Determine expected sequence length from ESP32 capture settings
        # Determine expected sequence length from ESP32 capture settings
        SEQ_LEN = int(1500 / 10)  # 1500ms/10ms = 150 samples
        if arr.ndim == 1:
            # single sample flatten: reshape to (1, seq_len, 3)
            if arr.size != SEQ_LEN * 3:
                return jsonify({'error': f'Invalid input length. Expected {SEQ_LEN*3}, got {arr.size}'}), 400
            arr = arr.reshape((1, SEQ_LEN, 3))
        elif arr.ndim == 2:
            # already as list of samples
            n_features = arr.shape[1]
            if n_features != SEQ_LEN * 3:
                return jsonify({'error': f'Invalid input shape. Expected (n, {SEQ_LEN*3}), got {arr.shape}'}), 400
            arr = arr.reshape((arr.shape[0], SEQ_LEN, 3))
        else:
            return jsonify({'error': 'Unsupported input dimensions'}), 400

        logger.info(f'Making prediction for input shape: {arr.shape}')
        preds = mock_predict(arr)

        # Map to gesture labels
        gesture_classes = ['O', 'V', 'Z']
        predicted_index = int(np.argmax(preds, axis=1)[0])
        confidence = float(np.max(preds) * 100)
        gesture = gesture_classes[predicted_index]

        return jsonify({'gesture': gesture, 'confidence': confidence})
    except Exception as e:
        logger.error(f'Prediction error: {e}')
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
