/*+
 * PROJECT:            3D-Deconvolution
 *    FILE:               DeconvolutionPlugin
 *
 * COPYRIGHT:          University of Massachusetts Medical School, 2020
 *    LICENSE:            LGPL
 *    LICENSE URL:        https://www.gnu.org/licenses/lgpl-3.0.txt
 *
 * AUTHOR:             Karl Bellve
 *    WEBSITE:            http://big.umassmed.edu
 *    EMAIL:              Karl.Bellve@umassmed.edu, Karl.Bellve@gmail.com
 */

package edu.umassmed.deconvolution;

import org.micromanager.MenuPlugin;
import org.micromanager.Studio;
import org.scijava.plugin.Plugin;
import org.scijava.plugin.SciJavaPlugin;

import javax.swing.*;
import java.awt.*;

@Plugin(type = MenuPlugin.class)
public class DeconvolutionPlugin implements SciJavaPlugin, MenuPlugin {

    // Provides access to the MicroManager API.

    private Studio studio_;
    private DeconvolutionFrame myFrame_;

    @Override
    public void setContext(Studio studio) {
        studio_ = studio;
    }
    /**
     * This method is called when the plugin's menu option is selected.
     */
    @Override
    public void onPluginSelected() {
        if (myFrame_ == null) {
            try {
                UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
                myFrame_ = new DeconvolutionFrame(studio_, this);
                myFrame_.setMinimumSize(new Dimension(325, 300));
                //myFrame_.pack();
                //myFrame_.setLocationRelativeTo(null);
            } catch (Exception e) {
                studio_.logs().logError(e);
                return;
            }
        }

        myFrame_.setVisible(true);
    }

    public void tellFrameClosed() { myFrame_ = null; }

    /**
     * This method determines which sub-menu of the Plugins menu we are placed
     * into.
     */
    @Override
    public String getSubMenu() {
        return "Beta";
        // Indicates that we should show up in the root Plugins menu.
//      return "";
    }

    @Override
    public String getName() {
        return "3D Deconvolution";
    }

    @Override
    public String getHelpText() {
        return "Deconvolves incoming images";
    }

    @Override
    public String getVersion() {
        return "1.0";
    }

    @Override
    public String getCopyright() {
        return "University of Massachusetts Medical School";
    }
}