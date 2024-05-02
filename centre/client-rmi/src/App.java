import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.rmi.Naming;

public class App {
    private static ICompte gestionCompte;

    private static boolean running = true;

    private static String gestion_compte_ip;
    private static int gestion_compte_port;
    private static int client_rmi_port;

    public static void main(String[] args) throws Exception {
        if (args.length != 3) {
            System.err.println(
                "Usage: java App <gestion-compte-ip> <gestion-compte-port> <client-rmi-port>");
            System.exit(1);
        }

        gestion_compte_ip = args[0];
        gestion_compte_port = Integer.parseInt(args[1]);
        client_rmi_port = Integer.parseInt(args[2]);

        // UDP server
        new Thread() {
            public void run() {
                while (running) {
                    client_rmi();
                    try {
                        Thread.sleep(1000);
                    } catch (Exception e) {
                        System.err.println("Error: " + e);
                        break;
                    }
                }
            }
        }.start();

        while (running)
            ;

        System.exit(0);
    }

    private static void client_rmi() {
        try {
            gestionCompte = (ICompte)Naming.lookup(String.format(
                "rmi://%s:%d/Compte", gestion_compte_ip, gestion_compte_port));
            System.out.println("client-rmi: connected to gestion-compte");
        } catch (Exception e) {
            System.out.println(
                "client-rmi: could not connect to gestion-compte");
        }

        byte[] data;
        DatagramPacket packet;
        DatagramSocket socket;

        try {
            data = new byte[256];
            packet = new DatagramPacket(data, data.length);
            socket = new DatagramSocket(client_rmi_port);
        } catch (Exception e) {
            System.err.println("client-rmi: " + e);
            return;
        }

        System.out.println("client-rmi: ready");

        try {
            while (running) {
                packet.setData(data);
                socket.receive(packet);

                String message =
                    new String(packet.getData(), 0, packet.getLength());

                System.out.println("client-rmi <-- " + message +
                                   " gestion-requete");

                String[] parts = message.split(":");
                String action = parts[0];

                String response = handle_action(action, parts);

                System.out.println("client-rmi --> " + response +
                                   " gestion-requete");

                packet.setData(response.getBytes());
                socket.send(packet);
            }
        } catch (Exception e) {
            System.err.println("client-rmi: " + e);
        }
        socket.close();
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