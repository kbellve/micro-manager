package edu.umassmed.deconvolution;

//import javax.swing.JFileChooser;
//import java.io.File;

import mmcorej.CMMCore;
import org.micromanager.Studio;
import org.micromanager.internal.utils.MMFrame;

import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;

public class DeconvolutionFrame extends MMFrame {

    int frameXPos_;
    int frameYPos_;

    double convergence_ = 0.001;
    int convergence_man_ = 1;
    int convergence_exp_ = -3;

    double smoothness_ = 0.001;
    int smoothness_man_ = 1;
    int smoothness_exp_ = -3;

    int iterations_ = 100;

    //Prefs
    // Mantissa
    private final static String CONVERGENCE_MAN = "CONVERGENCE_MAN";
    // Exponent
    private final static String CONVERGENCE_EXP = "CONVERGENCE_EXP";

    // Mantissa
    private final static String SMOOTHNESS_MAN = "SMOOTHNESS_MAN";
    // Exponent
    private final static String SMOOTHNESS_EXP = "SMOOTHNESS_EXP";

    private final static String ITERATIONS = "ITERATIONS";
    //

    private final Studio gui_;
    private final DeconvolutionPlugin parent_;

    // GUI
    private JPanel mainPanel;

    public JComboBox getPSFImageComboBox() {
        return PSFImageComboBox;
    }

    public void setPSFImageComboBox(JComboBox PSFImageComboBox) {
        this.PSFImageComboBox = PSFImageComboBox;
    }

    private JComboBox PSFImageComboBox;

    public JComboBox getInputImageComboBox() {
        return inputImageComboBox;
    }

    public void setInputImageComboBox(JComboBox inputImageComboBox) {
        this.inputImageComboBox = inputImageComboBox;
    }

    private JComboBox inputImageComboBox;

    public JFormattedTextField getConvergenceFormattedTextField() {
        return convergenceFormattedTextField;
    }

    public void setConvergenceFormattedTextField(JFormattedTextField convergenceFormattedTextField) {
        this.convergenceFormattedTextField = convergenceFormattedTextField;
    }

    private JFormattedTextField convergenceFormattedTextField;

    public JFormattedTextField getSmoothnessFormattedTextField() {
        return smoothnessFormattedTextField;
    }

    public void setSmoothnessFormattedTextField(JFormattedTextField smoothnessFormattedTextField) {
        this.smoothnessFormattedTextField = smoothnessFormattedTextField;
    }

    private JFormattedTextField smoothnessFormattedTextField;

    public JFormattedTextField getIterationsFormattedTextField() {
        return iterationsFormattedTextField;
    }

    public void setIterationsFormattedTextField(JFormattedTextField iterationsFormattedTextField) {
        this.iterationsFormattedTextField = iterationsFormattedTextField;
    }

    private JFormattedTextField iterationsFormattedTextField;
    private JPanel Settings;
    private JButton deconvolveButton;
    private JSpinner convergenceSpinner;
    private JSpinner smoothnessSpinner;
    private JSpinner iterationSpinner;
    private JSpinner spinner1;
    private JSpinner spinner2;


    public DeconvolutionFrame(Studio gui, DeconvolutionPlugin parent) {
        gui_ = gui;
        CMMCore core_ = gui.getCMMCore();
        parent_ = parent;

        initUIComponents();

        /* Set Default values of the spinners, before GUI is shown... */
        convergenceSpinner.setModel(new javax.swing.SpinnerNumberModel(Integer.valueOf(-2), Integer.valueOf(-5), Integer.valueOf(-2),Integer.valueOf(1)));
        convergenceSpinner.setMinimumSize(new java.awt.Dimension(50, 20));
        convergenceSpinner.setPreferredSize(new java.awt.Dimension(50, 20));

        smoothnessSpinner.setModel(new javax.swing.SpinnerNumberModel(Integer.valueOf(-2), Integer.valueOf(-5), Integer.valueOf(-2),Integer.valueOf(1)));
        smoothnessSpinner.setMinimumSize(new java.awt.Dimension(50, 20));
        smoothnessSpinner.setPreferredSize(new java.awt.Dimension(50, 20));

        iterationSpinner.setModel(new javax.swing.SpinnerNumberModel(Integer.valueOf(100), Integer.valueOf(1),  Integer.valueOf(1000), Integer.valueOf(1)));
        iterationSpinner.setMinimumSize(new java.awt.Dimension(50, 20));
        iterationSpinner.setPreferredSize(new java.awt.Dimension(50, 20));

        // place in center of screen
        Dimension screenSize = Toolkit.getDefaultToolkit().getScreenSize();
        frameXPos_ = screenSize.width / 2 - this.getSize().width / 2;
        frameYPos_ = screenSize.height / 2 - this.getSize().height / 2;
        super.loadAndRestorePosition(frameXPos_, frameYPos_);

        deconvolveButton.addActionListener(new ActionListener() {
            @Override
            public void actionPerformed(ActionEvent e) {
                gui_.logs().logMessage("Pressed Deconvolve Button");
            }


        });

    }
    private void initUIComponents() {

        add(mainPanel);
        setTitle("3D Deconvolution by BIG @ UMassMed");

        setDefaultCloseOperation(javax.swing.WindowConstants.DO_NOTHING_ON_CLOSE);

        final JFrame frame = this;
        this.addWindowListener(new WindowAdapter() {

            @Override
            public void windowClosing(WindowEvent e) {
                savePosition();
                saveSettings();
                parent_.tellFrameClosed();
                frame.setVisible(false);
                frame.dispose();
            }

        });

       // pack();

    }

    public void loadSettings() {
        smoothness_man_ = gui_.profile().getSettings(this.getClass()).getInteger(SMOOTHNESS_MAN, smoothness_man_);
        smoothness_exp_ = gui_.profile().getSettings(this.getClass()).getInteger(SMOOTHNESS_EXP, smoothness_exp_);

        convergence_man_ = gui_.profile().getSettings(this.getClass()).getInteger(CONVERGENCE_MAN, convergence_man_);
        convergence_exp_ = gui_.profile().getSettings(this.getClass()).getInteger(CONVERGENCE_EXP, convergence_exp_);

        iterations_ = gui_.profile().getSettings(this.getClass()).getInteger(ITERATIONS, iterations_);
    }


    public void saveSettings() {
        gui_.profile().getSettings(this.getClass()).putInteger(CONVERGENCE_MAN, convergence_man_);
        gui_.profile().getSettings(this.getClass()).putInteger(CONVERGENCE_EXP, convergence_exp_ );

        gui_.profile().getSettings(this.getClass()).putInteger(SMOOTHNESS_MAN, smoothness_man_);
        gui_.profile().getSettings(this.getClass()).putInteger(CONVERGENCE_EXP, smoothness_exp_);

        gui_.profile().getSettings(this.getClass()).putInteger(ITERATIONS, iterations_);
    }

    private void createUIComponents() {
        // TODO: place custom component creation code here
    }


}
