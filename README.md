- Install [Conan package manager](https://docs.conan.io/en/latest/introduction.html): `pip install conan`
- Install dependencies: `mkdir release && cd release && conan install ..`
- Build & run: `conan build .. && ./tp-endpoint-pov <hostname>`
