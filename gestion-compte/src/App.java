import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;
import java.util.HashMap;
import java.util.Map;

public class App extends UnicastRemoteObject implements ICompte {
    private static final String USERS_FILE = "users.txt";

    private static String gestion_compte_ip;
    private static int gestion_compte_port;

    private static Map<String, String> users;

    public App() throws RemoteException {
        super();
        App.users = new java.util.HashMap<>();
    }

    public static void main(String[] args) throws Exception {
        // if (args.length != 2) {
        //     System.out.println("Usage: java App <gestion-compte-ip>
        //     <gestion-compte-port>"); return;
        // }

        // gestion_compte_ip = args[0];
        // gestion_compte_port = Integer.parseInt(args[1]);

        App.gestion_compte_ip = "localhost";
        App.gestion_compte_port = 1099;

        App.users = new HashMap<>();

        System.out.print("\033[H\033[2J");
        System.out.println(String.format("%s:%d\tgestion-compte",
                                         App.gestion_compte_ip,
                                         App.gestion_compte_port));
        System.out.println("\n");

        // Create users file if it doesn't exist
        java.io.File file = new java.io.File(USERS_FILE);
        if (!file.exists()) {
            try {
                file.createNewFile();
            } catch (Exception e) {
                System.out.println("Failed to create users file: " + e);
            }
        }

        load();
        // System.out.println("Users loaded from file.");
        // for (Map.Entry<String, String> entry : users.entrySet()) {
        //     System.out.println(String.format("User: %s, Password: %s",
        //     entry.getKey(), entry.getValue()));
        // }

        try {
            App app = new App();
            java.rmi.registry.LocateRegistry.createRegistry(1099);
            java.rmi.Naming.rebind(String.format("rmi://localhost:1099/Compte"),
                                   app);
        } catch (Exception e) {
            System.out.println("Server failed: " + e);
        }
    }

    @Override
    public boolean register(String username, String password)
        throws RemoteException {
        if (users.containsKey(username))
            return false;
        users.put(username, password);
        for (Map.Entry<String, String> entry : users.entrySet()) {
            System.out.println(String.format("User: %s, Password: %s",
                                             entry.getKey(), entry.getValue()));
        }
        save();
        return true;
    }

    @Override
    public boolean login(String username, String password)
        throws RemoteException {
        if (!users.containsKey(username))
            return false;
        if (!users.get(username).equals(password))
            return false;
        return true;
    }

    @Override
    public boolean delete(String username, String password)
        throws RemoteException {
        if (!users.get(username).equals(password))
            return false;
        if (!users.containsKey(username))
            return false;
        users.remove(username);
        save();
        return true;
    }

    private static void save() {
        try (BufferedWriter writer =
                 new BufferedWriter(new FileWriter(USERS_FILE))) {
            for (Map.Entry<String, String> entry : users.entrySet()) {
                writer.write(entry.getKey() + ":" + entry.getValue());
                writer.newLine();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private static void load() {
        users.clear();
        try (BufferedReader reader =
                 new BufferedReader(new FileReader(USERS_FILE))) {
            String line;
            while ((line = reader.readLine()) != null) {
                String[] parts = line.split(":");
                if (parts.length == 2) {
                    users.put(parts[0], parts[1]);
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}