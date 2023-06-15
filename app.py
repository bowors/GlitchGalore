# from flask import Flask, request
# import json

# with open('index.html', 'r') as f:
#     html_content = f.read()

# app = Flask(__name__)

# @app.route('/', methods=['GET'])
# def home():
#      return html_content

# @app.route('/hook/<int:hook_id>', methods=['POST'])
# def index(hook_id):
#     # Notify process 2 by writing to the named pipe
#     with open('mypipe', 'w') as f:
#         f.write(f"request received from /hook/{hook_id}\n")
#     # Format JSON output nicely and print it
#     try:
#         data = json.loads(request.data)
#         print(json.dumps(data, indent=4))
#     except:
#         print(request.data)
    
#     return 'Hello world'

# if __name__ == '__main__':
#     app.run(port=5150)

print "asu"
