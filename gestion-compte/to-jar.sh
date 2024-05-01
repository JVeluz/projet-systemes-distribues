SRC_DIR="src"
BIN_DIR="bin"
JAR_FILE="gestion-compte.jar"
MAIN_CLASS="App"

javac -d "$BIN_DIR" "$SRC_DIR"/*.java
jar cvfe "$JAR_FILE" "$MAIN_CLASS" -C "$BIN_DIR" .
echo "Le fichier JAR \"$JAR_FILE\" a été créé avec succès."