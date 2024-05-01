import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.rmi.Naming;

public class App {
    private static ICompte gestionCompte;

    private static boolean running = true;
    private static boolean server_rmi_running = false;

    private static String gestion_compte_ip;
    private static int gestion_compte_port;
    private static String client_rmi_ip;
    private static int client_rmi_port;

    public static void main(String[] args) throws Exception {
        System.out.print("\033[H\033[2J");

        // if (args.length != 3) {
        //     System.out.println("Usage: java App <gestion_compte_ip>
        //     <gestion_compte_port> <client_rmi_port>"); System.exit(1);
        // }

        // App.gestion_compte_ip = args[0];
        // App.gestion_compte_port = Integer.parseInt(args[1]);
        // App.client_rmi_ip = Inet4Address.getLocalHost().getHostAddress();
        // App.client_rmi_port = Integer.parseInt(args[3]);

        App.gestion_compte_ip = "localhost";
        App.gestion_compte_port = 1099;
        App.client_rmi_ip = "localhost";
        App.client_rmi_port = 5000;

        System.out.println(String.format(
            "%s:%d\tgestion-compte", gestion_compte_ip, gestion_compte_port));
        System.out.println(
            String.format("%s:%d\tclient-rmi", client_rmi_ip, client_rmi_port));
        System.out.println("\n");

        // Reach RMI server
        new Thread() {
            public void run() {
                while (running) {
                    reach_server();
                    try {
                        Thread.sleep(5000);
                    } catch (Exception e) {
                    }
                }
            }
        }.start();

        // UDP server
        new Thread() {
            public void run() {
                while (running) {
                    client_rmi();
                    try {
                        Thread.sleep(5000);
                    } catch (Exception e) {
                    }
                }
            }
        }.start();

        String input = "";
        while (running) {
            input = System.console().readLine();
            if (input.equals("stop"))
                running = false;
        }

        System.exit(0);
    }

    private static void reach_server() {
        try {
            gestionCompte = (ICompte)Naming.lookup(String.format(
                "rmi://%s:%d/Compte", gestion_compte_ip, gestion_compte_port));
            if (!App.server_rmi_running)
                System.out.println("gestion-compte is running !");
            App.server_rmi_running = true;
        } catch (Exception e) {
            System.err.println("Can not reach gestion-compte...");
            App.server_rmi_running = false;
        }
    }

    private static void client_rmi() {
        try {
            byte[] data = new byte[256];
            DatagramSocket socket = new DatagramSocket(client_rmi_port);
            DatagramPacket packet = new DatagramPacket(data, data.length);

            while (running) {
                packet.setData(data);
                socket.receive(packet);

                String message =
                    new String(packet.getData(), 0, packet.getLength());

                System.out.println("<-- " + message + " gestion-requete");

                String[] parts = message.split(":");
                String action = parts[0];

                String response = handle_action(action, parts);

                System.out.println(response + " --> gestion-requete");

                packet.setData(response.getBytes());
                socket.send(packet);
            }
            socket.close();
        } catch (Exception e) {
            System.err.println("UDP server failed: " + e);
        }
    }

    private static String handle_action(String action, String[] parts) {
        String response = "failed";
        try {
            switch (action) {
            case "register":
                if (parts.length != 3)
                    break;
                if (gestionCompte.register(parts[1], parts[2]))
                    response = "success";
                break;
            case "login":
                if (parts.length != 3)
                    break;
                if (gestionCompte.login(parts[1], parts[2]))
                    response = "success";
                break;
            case "delete":
                if (parts.length != 3)
                    break;
                if (gestionCompte.delete(parts[1], parts[2]))
                    response = "success";
                break;
            default:
                break;
            }
        } catch (Exception e) {
            System.err.println("Error: " + e);
            response = "error";
        }
        return response;
    }
}